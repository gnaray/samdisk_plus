#pragma once

#include "Platform.h"
#include "IBMPCBase.h"
#include "Header.h"
#include "Track.h"

int GetFormatLength(Encoding encoding, int sectors, int size, int gap3);
int GetUnformatSizeCode(DataRate datarate);
int GetFormatGap(int drive_speed, DataRate datarate, Encoding encoding, int sectors, int size);

struct FitDetails
{
    int total_units = 0;
    int size_code = 0;
    int gap3 = 0;
    VectorX<int> sector_units{};
    bool real_errors;
};

bool FitTrackIBMPC(const CylHead& cylhead, const Track& track, int drive_speed, FitDetails& details);
