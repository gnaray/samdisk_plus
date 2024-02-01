// vfdrawtrack virtual device

#include "DemandDisk.h"

class VfdRawTrackDevDisk final : public DemandDisk
{
public:
    VfdRawTrackDevDisk(const std::string& path);

protected:
    bool preload(const Range& range, int cyl_step) override;
    bool is_constant_disk() const override;
    bool supports_retries() const override;
    bool supports_rescans() const override;
    TrackData load(const CylHead& cylhead, bool first_read,
        int with_head_seek_to, const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{}) override;

    TrackData LoadPhysicalTrack(const CylHead& cylhead);

private:
    void SetMetadata(const std::string& path);
};

bool ReadVfdrawtrack(const std::string& path, std::shared_ptr<Disk>& disk);
bool WriteVfdrawtrack(const std::string& path, std::shared_ptr<Disk>& disk);
