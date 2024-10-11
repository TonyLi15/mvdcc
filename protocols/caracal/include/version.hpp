#pragma once

class Version {
  public:
    enum class VersionStatus { PENDING, STABLE }; // status of version

    alignas(64) void *rec = nullptr; // nullptr if deleted = true (immutable)
    VersionStatus status;
    bool deleted; // (immutable)
};
