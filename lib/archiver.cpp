#include "archiver.hpp"

#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>

#include "init.hpp"

Archiver::Archiver(Logger logger, std::string bucket, std::string keyFormat)
    : _logger{std::move(logger)}, _bucket{std::move(bucket)}, _keyFormat{std::move(keyFormat)}
{
    _thread = std::thread([this] {
        _run();
    });
}

Archiver::~Archiver() {
    {
        std::lock_guard<std::mutex> l{_mutex};
        _isDestructing = true;
    }
    _cv.notify_one();
    _thread.join();
}

void Archiver::receiveEncodedAudioConfig(const void* data, size_t len) {
    _write(ArchiveDataType::AudioConfig, data, len);
}

void Archiver::receiveEncodedAudio(const void* data, size_t len) {
    _write(ArchiveDataType::Audio, data, len);
}

void Archiver::receiveEncodedVideoConfig(const void* data, size_t len) {
    _write(ArchiveDataType::VideoConfig, data, len);
}

void Archiver::receiveEncodedVideo(const void* data, size_t len) {
    _write(ArchiveDataType::Video, data, len);
}

void Archiver::_run() {
    _logger.info("archiver thread running");

    InitAWS();

    std::vector<uint8_t> uploadBuffer;

    int nextPartNumber = 0;
    Aws::String uploadId;

    size_t uploadCount = 0;
    Aws::String currentKey;

    Aws::S3::Model::CompletedMultipartUpload completedUpload;

    std::unique_lock<std::mutex> l{_mutex};
    while (true) {
        while (!_isDestructing && _buffer.size() < 5 * 1024 * 1024) {
            _cv.wait(l);
        }

        uploadBuffer.clear();
        uploadBuffer.swap(_buffer);
        l.unlock();

        if ((uploadBuffer.empty() && nextPartNumber > 1) || nextPartNumber > 20) {
            Aws::S3::Model::CompleteMultipartUploadRequest request;
            request.SetBucket(_bucket.c_str());
            request.SetKey(currentKey);
            request.SetUploadId(uploadId);
            request.SetMultipartUpload(completedUpload);

            if (auto outcome = _s3Client->CompleteMultipartUpload(request); outcome.IsSuccess()) {
                _logger.info("completed multipart upload");
            } else {
                _logger.error("unable to complete multipart upload: {}: {}", outcome.GetError().GetExceptionName(), outcome.GetError().GetMessage());
            }

            nextPartNumber = 0;
        }

        if (uploadBuffer.empty()) {
            break;
        }

        if (!nextPartNumber) {
            if (!_s3Client) {
                _s3Client = std::make_shared<Aws::S3::S3Client>();
            }

            currentKey = fmt::format(_keyFormat, uploadCount++);

            Aws::S3::Model::CreateMultipartUploadRequest request;
            request.SetBucket(_bucket.c_str());
            request.SetKey(currentKey);

            if (auto outcome = _s3Client->CreateMultipartUpload(request); outcome.IsSuccess()) {
                _logger.info("created new multipart upload");
                uploadId = outcome.GetResult().GetUploadId();
                completedUpload = {};
                nextPartNumber = 1;
            } else {
                _logger.error("unable to create multipart upload: {}: {}", outcome.GetError().GetExceptionName(), outcome.GetError().GetMessage());
            }
        }

        if (nextPartNumber) {
            Aws::S3::Model::UploadPartRequest request;
            request.SetBucket(_bucket.c_str());
            request.SetKey(currentKey);
            request.SetPartNumber(nextPartNumber);
            request.SetUploadId(uploadId);

            Aws::Utils::Array<uint8_t> array(uploadBuffer.data(), uploadBuffer.size());
            Aws::Utils::Stream::PreallocatedStreamBuf streamBuf(&array, array.GetLength());
            request.SetBody(std::make_shared<Aws::IOStream>(&streamBuf));

            if (auto outcome = _s3Client->UploadPart(request); outcome.IsSuccess()) {
                Aws::S3::Model::CompletedPart completedPart;
                completedPart.SetPartNumber(nextPartNumber);
                completedPart.SetETag(outcome.GetResult().GetETag());
                completedUpload.AddParts(completedPart);
                ++nextPartNumber;
            } else {
                _logger.error("unable to upload part: {}: {}", outcome.GetError().GetExceptionName(), outcome.GetError().GetMessage());
            }
        }

        l.lock();
    }

    _logger.info("archiver thread exiting");
}

void Archiver::_write(ArchiveDataType type, const void* data, size_t len) {
    auto steadyTime = std::chrono::steady_clock::now();
    auto systemTime = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> l{_mutex};
        auto offset = _buffer.size();
        auto totalLen = 1 + 16 + len;
        _buffer.resize(offset + 4 + totalLen);

        for (int i = 0; i < 4; ++i) {
            _buffer[offset++] = (totalLen >> ((3 - i) * 8)) & 0xff;
        }

        _buffer[offset++] = static_cast<uint8_t>(type);

        auto steadyNano = std::chrono::duration_cast<std::chrono::nanoseconds>(steadyTime.time_since_epoch()).count();
        for (int i = 0; i < 8; ++i) {
            _buffer[offset++] = (steadyNano >> ((7 - i) * 8)) & 0xff;
        }

        auto systemNano = std::chrono::duration_cast<std::chrono::nanoseconds>(systemTime.time_since_epoch()).count();
        for (int i = 0; i < 8; ++i) {
            _buffer[offset++] = (systemNano >> ((7 - i) * 8)) & 0xff;
        }

        memcpy(&_buffer[offset], data, len);
    }
    _cv.notify_one();
}
