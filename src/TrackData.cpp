#include "Options.h"
#include "TrackData.h"
#include "BitstreamDecoder.h"
#include "BitstreamEncoder.h"
#include "Util.h"

static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_prefer = getOpt<PreferredData>("prefer");

TrackData::TrackData(const CylHead& cylhead_)
    : cylhead(cylhead_)
{
}

TrackData::TrackData(const CylHead& cylhead_, Track&& track)
    : cylhead(cylhead_), m_type(TrackDataType::Track)
{
    add(std::move(track));
}

TrackData::TrackData(const CylHead& cylhead_, BitBuffer&& bitstream)
    : cylhead(cylhead_), m_type(TrackDataType::BitStream)
{
    add(std::move(bitstream));
}

TrackData::TrackData(const CylHead& cylhead_, FluxData&& flux, bool normalised)
    : cylhead(cylhead_), m_type(TrackDataType::Flux)
{
    add(std::move(flux), normalised);
}


bool TrackData::has_track() const
{
    return (m_flags & TD_TRACK) != 0;
}

bool TrackData::has_bitstream() const
{
    return (m_flags & TD_BITSTREAM) != 0;
}

bool TrackData::has_flux() const
{
    return (m_flags & TD_FLUX) != 0;
}

bool TrackData::has_normalised_flux() const
{
    return has_flux() && m_normalised_flux;
}


Track& TrackData::trackNC()
{
    if (!has_track())
    {
        if (!has_bitstream())
            bitstream();

        if (has_bitstream())
        {
            scan_bitstream(*this);
            m_flags |= TD_TRACK;
        }
    }

    return m_track;
}

const Track& TrackData::track()
{
    return trackNC();
}

/*const*/ BitBuffer& TrackData::bitstream()
{
    if (!has_bitstream())
    {
        if (has_track())
            generate_bitstream(*this);
        else if (has_flux())
            scan_flux(*this);
        else
        {
            add(Track());
            generate_bitstream(*this);
        }

        m_flags |= TD_BITSTREAM;
    }

    return m_bitstream;
}

FluxData& TrackData::fluxNC()
{
    if (!has_flux())
    {
        if (!has_bitstream())
            bitstream();

        if (has_bitstream())
        {
            generate_flux(*this);
            m_flags |= TD_FLUX;
        }
    }

    return m_flux;
}

const FluxData& TrackData::flux()
{
    return fluxNC();
}

TrackData TrackData::preferred()
{
    switch (opt_prefer)
    {
    case PreferredData::Track:
        return { cylhead, Track(track()) };
    case PreferredData::Bitstream:
        return { cylhead, BitBuffer(bitstream()) };
    case PreferredData::Flux:
        return { cylhead, FluxData(flux()) };
    case PreferredData::Unknown:
        break;
    }

    auto trackdata = *this;
    if (trackdata.has_flux() && !trackdata.has_normalised_flux())
    {
        // Ensure there are track and bitstream representations, then clear
        // the unnormalised flux, as its use must be explicitly requested.
        trackdata.track();
        trackdata.m_flux.clear();
        trackdata.m_flags &= ~TD_FLUX;
    }
    return trackdata;
}


void TrackData::add(TrackData&& trackdata)
{
    if (trackdata.has_flux())
        add(FluxData(trackdata.flux()), trackdata.has_normalised_flux());

    if (trackdata.has_bitstream())
        add(BitBuffer(trackdata.bitstream()));

    if (trackdata.has_track())
        add(Track(trackdata.track()));
}

void TrackData::add(Track&& track)
{
    if (!has_track())
    {
        m_track = std::move(track);
        m_flags |= TD_TRACK;
    }
    else
    {
        // Add new data to existing
        m_track.add(std::move(track));
    }
}

void TrackData::add(BitBuffer&& bitstream)
{
    m_bitstream = std::move(bitstream);
    m_flags |= TD_BITSTREAM;
}

void TrackData::add(FluxData&& flux, bool normalised)
{
    m_normalised_flux = normalised;
    m_flux = std::move(flux);
    m_flags |= TD_FLUX;
}

void TrackData::fix_track_readstats()
{
    for (auto& sector : trackNC().sectors())
        sector.fix_readstats();
}

void TrackData::ForceCylHeads(const int trackSup)
{
    if (!opt_normal_disk)
        return;
    auto& track = trackNC();
    for (auto& sector : track)
    {
        if (sector.header.operator CylHead() == cylhead)
            continue;
        if (!sector.header.IsNormal(trackSup))
            throw util::diskforeigncylhead_exception(cylhead, " does not match not normal cyl head of sector (", sector, ")");
        // Slight cyl mismatch in normal mode is a floppy drive and floppy disk incompatibility error.
        // It is sure sign of misreading from neighbor track. However if the
        // mismatching is bigger then it is probably a disk copy protection.
        if (sector.header.cyl != cylhead.cyl)
        {
            const auto msg = make_string("Suspicious: ", cylhead, "'s cyl does not match sector's cyl (", sector, ")");
            if (sector.HasNormalHeaderAndMisreadFromNeighborCyl(cylhead, trackSup))
                MessageCPP(msgWarningAlways, msg, ", probably the floppy disk is deformed");
            else
                MessageCPP(msgWarningAlways, msg, ", probably intended not normal sector header");
            if (sector.header.head != cylhead.head)
                MessageCPP(msgWarningAlways, "Suspicious: ", cylhead, "'s head does not match sector's head (", sector,
                    "), probably intended not normal sector header");
        }
        else if (sector.header.head != cylhead.head)
        {
            // In case the head is valid (0 or 1) the head mismatch is either
            // hardware error (the drive reads only from one head, and the user
            // will notice that data is same on head 0 and head 1),
            // or sign of wrong formatting. In both cases the head value is
            // adjusted to give it a chance.
            MessageCPP(msgWarningAlways, "Overriding wrong head of sector (", sector, ") with head ", cylhead.head);
            sector.header.head = cylhead.head;
        }
    }
}
