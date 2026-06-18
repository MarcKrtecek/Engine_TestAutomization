//-----------------------------------------------------------------------------
//   automization.cpp
//
//   Project: DRAINS ENGINE REGRESSION TESTING
//   Version: 1.0
//   Date:    15/06/26 
//   Author:  M. Benkert
//
//   Entry point for the automized regression testing of DRAINS engine functionality.
//
//   Watercom Update History
//   =======================
//   15/06/26: created
//
//-----------------------------------------------------------------------------

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <iomanip>
#include <windows.h>
#include <stdexcept>
#include <cctype>

#include "tolerances.h"

namespace fs = std::filesystem;

constexpr int32_t MAGICNUMBER = 516114522;
constexpr int DateDelta = 693594;
constexpr int SecsPerDay = 86400;
constexpr int DATE_STR_SIZE = 32;
constexpr int TIME_STR_SIZE = 16;
const char* const MonthTxt[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
const int DaysPerMonth[2][12] =
{
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

struct DateTime
{
    double value;

    DateTime() : value(0.0) {}
    DateTime(double v) : value(v) {}
    operator double() const { return value; }
};

void divMod(int numerator, int denominator, int* quotient, int* remainder)
{
    *quotient = numerator / denominator;
    *remainder = numerator % denominator;
}

int isLeapYear(int year)
{
    if (year % 4 != 0) return 0;
    if (year % 100 != 0) return 1;
    return year % 400 == 0;
}

void datetime_decodeDate(DateTime date, int* year, int* month, int* day)
{
    constexpr int D1 = 365;
    constexpr int D4 = D1 * 4 + 1;
    constexpr int D100 = D4 * 25 - 1;
    constexpr int D400 = D100 * 4 + 1;

    int t = static_cast<int>(std::floor(date.value)) + DateDelta;
    if (t <= 0)
    {
        *year = 0;
        *month = 1;
        *day = 1;
        return;
    }

    --t;
    int y = 1;
    while (t >= D400)
    {
        t -= D400;
        y += 400;
    }

    int i = 0;
    int d = 0;
    divMod(t, D100, &i, &d);
    if (i == 4)
    {
        --i;
        d += D100;
    }

    y += i * 100;
    divMod(d, D4, &i, &d);
    y += i * 4;
    divMod(d, D1, &i, &d);
    if (i == 4)
    {
        --i;
        d += D1;
    }

    y += i;
    const int leap = isLeapYear(y);
    int m = 1;
    for (;;)
    {
        i = DaysPerMonth[leap][m - 1];
        if (d < i) break;
        d -= i;
        ++m;
    }

    *year = y;
    *month = m;
    *day = d + 1;
}

void datetime_decodeTime(DateTime time, int* h, int* m, int* s)
{
    int mins = 0;
    const double fracDay = (time.value - std::floor(time.value)) * SecsPerDay;
    int secs = static_cast<int>(std::floor(fracDay + 0.5));
    if (secs >= SecsPerDay) secs = SecsPerDay - 1;
    divMod(secs, 60, &mins, s);
    divMod(mins, 60, h, m);
    if (*h > 23) *h = 0;
}

void datetime_dateToStr(DateTime date, char* s)
{
    int year = 0;
    int month = 1;
    int day = 1;
    datetime_decodeDate(date, &year, &month, &day);
    snprintf(s, DATE_STR_SIZE, "%04d-%3s-%02d", year, MonthTxt[month - 1], day);
}

void datetime_timeToStr(DateTime time, char* s)
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    datetime_decodeTime(time, &hour, &minute, &second);
    snprintf(s, TIME_STR_SIZE, "%02d:%02d:%02d", hour, minute, second);
}

std::string formatDateTime(double days)
{
    char date[DATE_STR_SIZE]{};
    char time[TIME_STR_SIZE]{};
    DateTime dateTime(days);
    datetime_dateToStr(dateTime, date);
    datetime_timeToStr(dateTime, time);
    return std::string(date) + " " + std::string(time);
}

enum ResultElementType : int
{
    RESULT_SUBCATCHMENT = 0,
    RESULT_HYDROLOGY_LINK = 1,
    RESULT_HYDROLOGY_NODE = 2,
    RESULT_HYDROLOGY_WSUD = 3,
    RESULT_HYDRAULIC_NODE = 4,
    RESULT_HYDRAULIC_LINK = 5
};

struct TimeSeriesResultRecord
{
    std::string id;
    int type = 0;
    std::vector<int32_t> variableCodes;
    std::vector<double> times;
    std::vector<std::vector<float>> values;
};

struct BufferedTimeSeriesResults
{
    std::vector<TimeSeriesResultRecord> records;
};

struct SubcatchAnnualRunoffSummary
{
    double imperviousRunoff = 0.0;
    double perviousRunoff = 0.0;
    double totalRunoff = 0.0;
};

struct HydrologyNodeSummary
{
    double totalInflowVolume = 0.0;
};

struct HydrologyLinkSummary
{
    double totalInflowVolume = 0.0;
    double totalOutflowVolume = 0.0;
};

struct HydrologyWSUDSummary
{
    double volumeInflow = 0.0;
    double volumeLostFromSystem = 0.0;
    double volumeNetworkOutflow = 0.0;
    double volumeWithdrawn = 0.0;
    double volumeRequested = 0.0;
    double volumeLowFlowBypass = 0.0;
    double volumeHighFlowBypass = 0.0;
    double volumeEvaporation = 0.0;
    double volumeExfiltration = 0.0;
    double volumeTreatedFlow = 0.0;
    double volumeOverflow = 0.0;
    double treatmentTrainSourceFlow = 0.0;
    double treatmentTrainResidualFlow = 0.0;
};

struct HydraulicNodeSummary
{
    double avgDepth = 0.0;
    double maxDepth = 0.0;
    double maxLatFlow = 0.0;
    double maxInflow = 0.0;
    double totalLatFlow = 0.0;
    double totalInflow = 0.0;
    double continuityError = 0.0;
    double volumeFlooded = 0.0;
};

struct HydraulicLinkSummary
{
    double maxFlow = 0.0;
    double maxVelocity = 0.0;
    double maxFlowRatio = 0.0;
};

class BinaryReader
{
public:
    explicit BinaryReader(const fs::path& path) : path_(path), input_(path, std::ios::binary)
    {
        if (!input_)
            throw std::runtime_error("Could not open binary result file: " + path.string());
    }

    int64_t fileSize()
    {
        input_.seekg(0, std::ios::end);
        return static_cast<int64_t>(input_.tellg());
    }

    void seek(int64_t position)
    {
        input_.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        if (!input_)
            throw std::runtime_error("Could not seek in binary result file: " + path_.string());
    }

    int64_t tell()
    {
        return static_cast<int64_t>(input_.tellg());
    }

    template <typename T>
    T read()
    {
        T value{};
        input_.read(reinterpret_cast<char*>(&value), sizeof(T));
        if (!input_)
            throw std::runtime_error("Could not read binary result file: " + path_.string());
        return value;
    }

    std::string readID()
    {
        const int32_t size = read<int32_t>();
        if (size < 0)
            throw std::runtime_error("Invalid ID length in binary result file: " + path_.string());

        std::string id(static_cast<size_t>(size), '\0');
        if (size > 0)
            input_.read(id.data(), size);
        if (!input_)
            throw std::runtime_error("Could not read ID in binary result file: " + path_.string());

        return id;
    }

    std::vector<float> readFloats(int32_t count)
    {
        if (count < 0)
            throw std::runtime_error("Invalid float count in binary result file: " + path_.string());

        std::vector<float> values(static_cast<size_t>(count));
        if (count > 0)
            input_.read(reinterpret_cast<char*>(values.data()), sizeof(float) * count);
        if (!input_)
            throw std::runtime_error("Could not read float block in binary result file: " + path_.string());

        return values;
    }

private:
    fs::path path_;
    std::ifstream input_;
};

std::vector<int32_t> readVariableCodes(BinaryReader& reader, int32_t count)
{
    std::vector<int32_t> codes;
    codes.reserve(static_cast<size_t>((std::max)(count, 0)));

    for (int32_t i = 0; i < count; ++i)
        codes.push_back(reader.read<int32_t>());

    return codes;
}

std::vector<int32_t> makeSequentialVariableCodes(int32_t count)
{
    std::vector<int32_t> codes;
    codes.reserve(static_cast<size_t>((std::max)(count, 0)));

    for (int32_t i = 0; i < count; ++i)
        codes.push_back(i);

    return codes;
}

int64_t findSystemVariableBlockStart(
    BinaryReader& reader,
    int64_t searchStart,
    int64_t outputStartPos,
    int32_t& systemVarCount)
{
    const int64_t systemBlockEnd = outputStartPos - static_cast<int64_t>(sizeof(double) + sizeof(int32_t));
    if (searchStart < 0 || searchStart > systemBlockEnd)
        throw std::runtime_error("Invalid system result variable search range in binary result file.");

    for (int64_t position = searchStart; position <= systemBlockEnd - static_cast<int64_t>(sizeof(int32_t)); position += sizeof(int32_t))
    {
        const int64_t remainingBytes =
            systemBlockEnd - position - static_cast<int64_t>(sizeof(int32_t));
        if (remainingBytes < 0 || remainingBytes % static_cast<int64_t>(sizeof(int32_t)) != 0)
            continue;

        const int64_t expectedCount64 = remainingBytes / static_cast<int64_t>(sizeof(int32_t));
        if (expectedCount64 > INT32_MAX)
            continue;

        reader.seek(position);
        const int32_t count = reader.read<int32_t>();
        if (count != static_cast<int32_t>(expectedCount64))
            continue;

        bool codesMatch = true;
        for (int32_t code = 0; code < count; ++code)
        {
            if (reader.read<int32_t>() != code)
            {
                codesMatch = false;
                break;
            }
        }

        if (codesMatch)
        {
            systemVarCount = count;
            return position;
        }
    }

    throw std::runtime_error("Could not locate system result variable block in binary result file.");
}

int64_t checkedBytesPerPeriod(int64_t outputStartPos, int32_t periods, int64_t firstSummaryPos)
{
    if (periods < 0)
        throw std::runtime_error("Invalid negative period count in binary result file.");
    if (periods == 0)
        return 0;
    if (firstSummaryPos < outputStartPos)
        throw std::runtime_error("Invalid result offsets in binary result file.");

    const int64_t timeSeriesBytes = firstSummaryPos - outputStartPos;
    if (timeSeriesBytes % periods != 0)
        throw std::runtime_error("Time-series block size is not divisible by the period count.");

    return timeSeriesBytes / periods;
}

void addRecords(
    BufferedTimeSeriesResults& results,
    const std::vector<std::string>& ids,
    int type,
    const std::vector<int32_t>& variableCodes)
{
    for (const std::string& id : ids)
    {
        TimeSeriesResultRecord record;
        record.id = id;
        record.type = type;
        record.variableCodes = variableCodes;
        results.records.push_back(std::move(record));
    }
}

struct RoutingBinaryLayout
{
    int32_t nodeCount = 0;
    int32_t linkCount = 0;
    int32_t pollutantCount = 0;
    int32_t nodeVarCount = 0;
    int32_t linkVarCount = 0;
    int32_t systemVarCount = 0;
    int32_t periodCount = 0;
    int64_t inputStartPos = 0;
    int64_t outputStartPos = 0;
    int64_t nodeSummaryStartPos = 0;
    int64_t linkSummaryStartPos = 0;
    int64_t bytesPerPeriod = 0;
    std::vector<std::string> nodeIDs;
    std::vector<std::string> linkIDs;
    std::vector<int32_t> nodeVariableCodes;
    std::vector<int32_t> linkVariableCodes;
};

void readRoutingFooter(BinaryReader& reader, RoutingBinaryLayout& layout)
{
    const int64_t fileSize = reader.fileSize();
    const int64_t footerSizeWithSummaryOffsets =
        5 * static_cast<int64_t>(sizeof(int64_t)) + 3 * static_cast<int64_t>(sizeof(int32_t));

    if (fileSize < footerSizeWithSummaryOffsets)
        throw std::runtime_error("Routing binary is too small to contain a valid footer.");

    reader.seek(fileSize - footerSizeWithSummaryOffsets);
    reader.read<int64_t>(); // IDStartPos
    layout.inputStartPos = reader.read<int64_t>();
    layout.outputStartPos = reader.read<int64_t>();
    layout.nodeSummaryStartPos = reader.read<int64_t>();
    layout.linkSummaryStartPos = reader.read<int64_t>();
    layout.periodCount = reader.read<int32_t>();
    reader.read<int32_t>(); // ErrorCode

    if (reader.read<int32_t>() != MAGICNUMBER)
        throw std::runtime_error("Routing binary has an invalid footer magic number.");

    if (layout.inputStartPos <= 0 ||
        layout.outputStartPos <= layout.inputStartPos ||
        layout.outputStartPos >= fileSize ||
        layout.nodeSummaryStartPos < layout.outputStartPos ||
        layout.nodeSummaryStartPos >= fileSize ||
        layout.linkSummaryStartPos < layout.nodeSummaryStartPos ||
        layout.linkSummaryStartPos >= fileSize ||
        layout.periodCount < 0)
        throw std::runtime_error("Routing binary has an invalid footer.");
}

struct RunoffBinaryLayout
{
    int32_t subcatchCount = 0;
    int32_t hydrologyLinkCount = 0;
    int32_t hydrologyNodeCount = 0;
    int32_t wsudCount = 0;
    int32_t pollutantCount = 0;
    int32_t subcatchVarCount = 0;
    int32_t hydrologyLinkVarCount = 0;
    int32_t hydrologyNodeVarCount = 0;
    int32_t wsudVarCount = 0;
    int32_t systemVarCount = 0;
    int32_t periodCount = 0;
    int64_t inputStartPos = 0;
    int64_t outputStartPos = 0;
    int64_t catchmentSummaryStartPos = 0;
    int64_t hydrologyNodeSummaryStartPos = 0;
    int64_t hydrologyLinkSummaryStartPos = 0;
    int64_t hydrologyWSUDSummaryStartPos = 0;
    int64_t bytesPerPeriod = 0;
    std::vector<std::string> subcatchIDs;
    std::vector<std::string> hydrologyLinkIDs;
    std::vector<std::string> hydrologyNodeIDs;
    std::vector<std::string> wsudIDs;
    std::vector<int32_t> subcatchVariableCodes;
    std::vector<int32_t> hydrologyLinkVariableCodes;
    std::vector<int32_t> hydrologyNodeVariableCodes;
    std::vector<int32_t> wsudVariableCodes;
};

struct RunoffBinaryResults
{
    RunoffBinaryLayout layout;
    std::vector<TimeSeriesResultRecord> subcatchmentRecords;
    std::vector<TimeSeriesResultRecord> hydrologyNodeRecords;
    std::vector<TimeSeriesResultRecord> hydrologyLinkRecords;
    std::vector<TimeSeriesResultRecord> hydrologyWSUDRecords;
    std::vector<SubcatchAnnualRunoffSummary> subcatchmentSummaries;
    std::vector<HydrologyNodeSummary> hydrologyNodeSummaries;
    std::vector<HydrologyLinkSummary> hydrologyLinkSummaries;
    std::vector<HydrologyWSUDSummary> hydrologyWSUDSummaries;
};

struct RoutingBinaryResults
{
    RoutingBinaryLayout layout;
    std::vector<TimeSeriesResultRecord> hydraulicNodeRecords;
    std::vector<TimeSeriesResultRecord> hydraulicLinkRecords;
    std::vector<HydraulicNodeSummary> hydraulicNodeSummaries;
    std::vector<HydraulicLinkSummary> hydraulicLinkSummaries;
};

RoutingBinaryLayout readRoutingBinaryLayout(BinaryReader& reader)
{
    RoutingBinaryLayout layout;

    reader.seek(0);
    const int32_t magic = reader.read<int32_t>();
    if (magic != MAGICNUMBER)
        throw std::runtime_error("Routing binary has an invalid magic number.");

    reader.read<int32_t>(); // version
    reader.read<int32_t>(); // flow units
    layout.nodeCount = reader.read<int32_t>();
    layout.linkCount = reader.read<int32_t>();
    layout.pollutantCount = reader.read<int32_t>();

    for (int32_t i = 0; i < layout.nodeCount; ++i)
        layout.nodeIDs.push_back(reader.readID());
    for (int32_t i = 0; i < layout.linkCount; ++i)
        layout.linkIDs.push_back(reader.readID());
    for (int32_t i = 0; i < layout.pollutantCount; ++i)
        reader.readID();

    readRoutingFooter(reader, layout);

    const int64_t nodeInputBytes =
        static_cast<int64_t>(sizeof(int32_t)) + 3 * static_cast<int64_t>(sizeof(int32_t)) +
        static_cast<int64_t>(layout.nodeCount) *
            (static_cast<int64_t>(sizeof(int32_t)) + 2 * static_cast<int64_t>(sizeof(float)));
    const int64_t linkInputBytes =
        static_cast<int64_t>(sizeof(int32_t)) + 5 * static_cast<int64_t>(sizeof(int32_t)) +
        static_cast<int64_t>(layout.linkCount) *
            (static_cast<int64_t>(sizeof(int32_t)) + 4 * static_cast<int64_t>(sizeof(float)));

    reader.seek(layout.inputStartPos + nodeInputBytes + linkInputBytes);
    layout.nodeVarCount = reader.read<int32_t>();
    layout.nodeVariableCodes = readVariableCodes(reader, layout.nodeVarCount);
    layout.linkVarCount = reader.read<int32_t>();
    layout.linkVariableCodes = readVariableCodes(reader, layout.linkVarCount);
    const int64_t systemVariableBlockStart =
        findSystemVariableBlockStart(reader, reader.tell(), layout.outputStartPos, layout.systemVarCount);
    reader.seek(systemVariableBlockStart + static_cast<int64_t>(sizeof(int32_t)));
    readVariableCodes(reader, layout.systemVarCount);

    layout.bytesPerPeriod =
        static_cast<int64_t>(sizeof(double)) +
        static_cast<int64_t>(
            layout.nodeCount * layout.nodeVarCount +
            layout.linkCount * layout.linkVarCount +
            layout.systemVarCount) * static_cast<int64_t>(sizeof(float));

    if (layout.nodeSummaryStartPos > 0 &&
        layout.periodCount > 0 &&
        layout.outputStartPos + static_cast<int64_t>(layout.periodCount) * layout.bytesPerPeriod >
            layout.nodeSummaryStartPos)
    {
        throw std::runtime_error("Routing binary time-series block overlaps the summary block.");
    }

    return layout;
}

RunoffBinaryLayout readRunoffBinaryLayout(BinaryReader& reader)
{
    RunoffBinaryLayout layout;

    reader.seek(0);
    const int32_t magic = reader.read<int32_t>();
    if (magic != MAGICNUMBER)
        throw std::runtime_error("Runoff binary has an invalid magic number.");

    reader.read<int32_t>(); // version
    reader.read<int32_t>(); // flow units
    layout.subcatchCount = reader.read<int32_t>();
    layout.hydrologyLinkCount = reader.read<int32_t>();
    layout.hydrologyNodeCount = reader.read<int32_t>();
    layout.wsudCount = reader.read<int32_t>();
    layout.pollutantCount = reader.read<int32_t>();

    for (int32_t i = 0; i < layout.subcatchCount; ++i)
        layout.subcatchIDs.push_back(reader.readID());
    for (int32_t i = 0; i < layout.hydrologyLinkCount; ++i)
        layout.hydrologyLinkIDs.push_back(reader.readID());
    for (int32_t i = 0; i < layout.hydrologyNodeCount; ++i)
        layout.hydrologyNodeIDs.push_back(reader.readID());
    for (int32_t i = 0; i < layout.wsudCount; ++i)
        layout.wsudIDs.push_back(reader.readID());
    for (int32_t i = 0; i < layout.pollutantCount; ++i)
        reader.readID();

    const int64_t fileSize = reader.fileSize();
    const int64_t footerSize =
        7 * static_cast<int64_t>(sizeof(int64_t)) + 2 * static_cast<int64_t>(sizeof(int32_t));
    reader.seek(fileSize - footerSize);
    reader.read<int64_t>(); // IDStartPosRunoff
    layout.inputStartPos = reader.read<int64_t>();
    layout.outputStartPos = reader.read<int64_t>();
    layout.catchmentSummaryStartPos = reader.read<int64_t>();
    layout.hydrologyNodeSummaryStartPos = reader.read<int64_t>();
    layout.hydrologyLinkSummaryStartPos = reader.read<int64_t>();
    layout.hydrologyWSUDSummaryStartPos = reader.read<int64_t>();
    layout.periodCount = reader.read<int32_t>();
    if (reader.read<int32_t>() != MAGICNUMBER)
        throw std::runtime_error("Runoff binary has an invalid footer magic number.");

    reader.seek(layout.inputStartPos + static_cast<int64_t>(sizeof(int32_t)) +
        static_cast<int64_t>(sizeof(int32_t)) +
        static_cast<int64_t>(layout.subcatchCount) * static_cast<int64_t>(sizeof(float)));
    layout.subcatchVarCount = reader.read<int32_t>();

    const int64_t subcatchVariableCodeStart = reader.tell();
    const int64_t systemVariableBlockStart =
        findSystemVariableBlockStart(reader, subcatchVariableCodeStart, layout.outputStartPos, layout.systemVarCount);
    const int64_t subcatchVariableCodeBytes = systemVariableBlockStart - subcatchVariableCodeStart;
    if (subcatchVariableCodeBytes < 0 ||
        subcatchVariableCodeBytes % static_cast<int64_t>(sizeof(int32_t)) != 0)
    {
        throw std::runtime_error("Invalid runoff subcatchment variable code block size.");
    }

    const int32_t subcatchVariableCodeCount =
        static_cast<int32_t>(subcatchVariableCodeBytes / static_cast<int64_t>(sizeof(int32_t)));
    reader.seek(subcatchVariableCodeStart);
    layout.subcatchVariableCodes = readVariableCodes(reader, subcatchVariableCodeCount);

    if (subcatchVariableCodeCount != layout.subcatchVarCount)
        layout.subcatchVariableCodes = makeSequentialVariableCodes(layout.subcatchVarCount);

    layout.hydrologyLinkVarCount = 2 + layout.pollutantCount;
    layout.hydrologyNodeVarCount = 1 + layout.pollutantCount;
    layout.wsudVarCount = 12 + 7 * layout.pollutantCount;
    layout.hydrologyLinkVariableCodes = makeSequentialVariableCodes(layout.hydrologyLinkVarCount);
    layout.hydrologyNodeVariableCodes = makeSequentialVariableCodes(layout.hydrologyNodeVarCount);
    layout.wsudVariableCodes = makeSequentialVariableCodes(layout.wsudVarCount);

    layout.bytesPerPeriod =
        checkedBytesPerPeriod(layout.outputStartPos, layout.periodCount, layout.catchmentSummaryStartPos);

    if (layout.periodCount > 0)
    {
        const int64_t expectedBytesPerPeriod =
            static_cast<int64_t>(sizeof(double)) +
            static_cast<int64_t>(
                layout.subcatchCount * layout.subcatchVarCount +
                layout.hydrologyLinkCount * layout.hydrologyLinkVarCount +
                layout.hydrologyNodeCount * layout.hydrologyNodeVarCount +
                layout.wsudCount * layout.wsudVarCount +
                layout.systemVarCount) * static_cast<int64_t>(sizeof(float));

        if (expectedBytesPerPeriod != layout.bytesPerPeriod)
            throw std::runtime_error("Runoff binary time-series layout does not match the inferred result variable counts.");
    }

    return layout;
}

void readRoutingTimeSeries(
    BinaryReader& reader,
    const RoutingBinaryLayout& layout,
    BufferedTimeSeriesResults& results)
{
    const size_t firstNodeRecord = results.records.size();
    addRecords(results, layout.nodeIDs, RESULT_HYDRAULIC_NODE, layout.nodeVariableCodes);
    const size_t firstLinkRecord = results.records.size();
    addRecords(results, layout.linkIDs, RESULT_HYDRAULIC_LINK, layout.linkVariableCodes);

    for (int32_t period = 0; period < layout.periodCount; ++period)
    {
        reader.seek(layout.outputStartPos + static_cast<int64_t>(period) * layout.bytesPerPeriod);
        const double date = reader.read<double>();

        for (int32_t i = 0; i < layout.nodeCount; ++i)
        {
            TimeSeriesResultRecord& record = results.records[firstNodeRecord + i];
            record.times.push_back(date);
            record.values.push_back(reader.readFloats(layout.nodeVarCount));
        }

        for (int32_t i = 0; i < layout.linkCount; ++i)
        {
            TimeSeriesResultRecord& record = results.records[firstLinkRecord + i];
            record.times.push_back(date);
            record.values.push_back(reader.readFloats(layout.linkVarCount));
        }

        reader.readFloats(layout.systemVarCount);
    }
}

std::vector<HydraulicNodeSummary> readHydraulicNodeSummaries(
    BinaryReader& reader,
    const RoutingBinaryLayout& layout)
{
    constexpr int valuesPerHydraulicNodeSummaryBeforePollutants = 36;

    std::vector<HydraulicNodeSummary> summaries;
    summaries.reserve(static_cast<size_t>(layout.nodeCount));

    for (int32_t i = 0; i < layout.nodeCount; ++i)
    {
        const int64_t position =
            layout.nodeSummaryStartPos +
            static_cast<int64_t>(i) * valuesPerHydraulicNodeSummaryBeforePollutants *
                static_cast<int64_t>(sizeof(double));

        reader.seek(position);

        HydraulicNodeSummary summary;
        summary.avgDepth = reader.read<double>();       // index 0
        summary.maxDepth = reader.read<double>();       // index 1
        reader.read<double>();                          // max head
        reader.read<double>();                          // time of max depth
        reader.read<double>();                          // max reported depth
        summary.maxLatFlow = reader.read<double>();     // index 5
        summary.maxInflow = reader.read<double>();      // index 6
        reader.read<double>();                          // max inflow date
        summary.totalLatFlow = reader.read<double>();   // index 8
        summary.totalInflow = reader.read<double>();    // index 9
        summary.continuityError = reader.read<double>(); // index 10
        for (int skip = 11; skip < 17; ++skip)
            reader.read<double>();
        summary.volumeFlooded = reader.read<double>();  // index 17

        summaries.push_back(summary);
    }

    return summaries;
}

std::vector<HydraulicLinkSummary> readHydraulicLinkSummaries(
    BinaryReader& reader,
    const RoutingBinaryLayout& layout)
{
    std::vector<HydraulicLinkSummary> summaries;
    summaries.reserve(static_cast<size_t>(layout.linkCount));

    int64_t position = layout.linkSummaryStartPos;
    for (int32_t i = 0; i < layout.linkCount; ++i)
    {
        reader.seek(position);

        HydraulicLinkSummary summary;
        summary.maxFlow = reader.read<double>();        // index 0
        reader.read<double>();                          // max flow date
        summary.maxVelocity = reader.read<double>();    // index 2
        summary.maxFlowRatio = reader.read<double>();   // index 3
        summaries.push_back(summary);

        // Only these first fixed values are needed for now. Advance to the
        // next link by consuming the rest of the variable-size link block.
        reader.read<double>(); // max depth ratio
        for (int skip = 0; skip < 3; ++skip)
            reader.read<double>(); // first three street summary doubles

        const int32_t inletNameLength = reader.read<int32_t>();
        if (inletNameLength < 0)
            throw std::runtime_error("Invalid inlet name length in routing summary.");
        reader.seek(reader.tell() + inletNameLength);

        const int32_t inletLocationLength = reader.read<int32_t>();
        if (inletLocationLength < 0)
            throw std::runtime_error("Invalid inlet location length in routing summary.");
        reader.seek(reader.tell() + inletLocationLength);

        for (int skip = 0; skip < 28; ++skip)
            reader.read<double>();

        if (layout.pollutantCount > 0)
        {
            for (int skip = 0; skip < 2 * layout.pollutantCount; ++skip)
                reader.read<double>();
        }

        position = reader.tell();
    }

    return summaries;
}

RoutingBinaryResults readRoutingBinaryResults(const fs::path& routingBinaryFile)
{
    RoutingBinaryResults results;
    BinaryReader reader(routingBinaryFile);
    results.layout = readRoutingBinaryLayout(reader);

    BufferedTimeSeriesResults bufferedResults;
    readRoutingTimeSeries(reader, results.layout, bufferedResults);
    for (TimeSeriesResultRecord& record : bufferedResults.records)
    {
        if (record.type == RESULT_HYDRAULIC_NODE)
            results.hydraulicNodeRecords.push_back(std::move(record));
        else if (record.type == RESULT_HYDRAULIC_LINK)
            results.hydraulicLinkRecords.push_back(std::move(record));
    }

    results.hydraulicNodeSummaries = readHydraulicNodeSummaries(reader, results.layout);
    results.hydraulicLinkSummaries = readHydraulicLinkSummaries(reader, results.layout);
    return results;
}

void readRunoffTimeSeries(
    BinaryReader& reader,
    const RunoffBinaryLayout& layout,
    BufferedTimeSeriesResults& results)
{
    const size_t firstSubcatchRecord = results.records.size();
    addRecords(results, layout.subcatchIDs, RESULT_SUBCATCHMENT, layout.subcatchVariableCodes);
    const size_t firstHydrologyLinkRecord = results.records.size();
    addRecords(results, layout.hydrologyLinkIDs, RESULT_HYDROLOGY_LINK, layout.hydrologyLinkVariableCodes);
    const size_t firstHydrologyNodeRecord = results.records.size();
    addRecords(results, layout.hydrologyNodeIDs, RESULT_HYDROLOGY_NODE, layout.hydrologyNodeVariableCodes);
    const size_t firstWsudRecord = results.records.size();
    addRecords(results, layout.wsudIDs, RESULT_HYDROLOGY_WSUD, layout.wsudVariableCodes);

    for (int32_t period = 0; period < layout.periodCount; ++period)
    {
        reader.seek(layout.outputStartPos + static_cast<int64_t>(period) * layout.bytesPerPeriod);
        const double date = reader.read<double>();

        for (int32_t i = 0; i < layout.subcatchCount; ++i)
        {
            TimeSeriesResultRecord& record = results.records[firstSubcatchRecord + i];
            record.times.push_back(date);
            record.values.push_back(reader.readFloats(layout.subcatchVarCount));
        }

        for (int32_t i = 0; i < layout.hydrologyLinkCount; ++i)
        {
            TimeSeriesResultRecord& record = results.records[firstHydrologyLinkRecord + i];
            record.times.push_back(date);
            record.values.push_back(reader.readFloats(layout.hydrologyLinkVarCount));
        }

        for (int32_t i = 0; i < layout.hydrologyNodeCount; ++i)
        {
            TimeSeriesResultRecord& record = results.records[firstHydrologyNodeRecord + i];
            record.times.push_back(date);
            record.values.push_back(reader.readFloats(layout.hydrologyNodeVarCount));
        }

        for (int32_t i = 0; i < layout.wsudCount; ++i)
        {
            TimeSeriesResultRecord& record = results.records[firstWsudRecord + i];
            record.times.push_back(date);
            record.values.push_back(reader.readFloats(layout.wsudVarCount));
        }

        reader.readFloats(layout.systemVarCount);
    }
}

std::vector<SubcatchAnnualRunoffSummary> readSubcatchAnnualRunoffSummaries(
    BinaryReader& reader,
    const RunoffBinaryLayout& layout)
{
    constexpr int catchmentSummaryAnnualImperviousIndex = 26;
    const int valuesPerCatchmentSummary = 64 + 6 * layout.pollutantCount;

    std::vector<SubcatchAnnualRunoffSummary> summaries;
    summaries.reserve(static_cast<size_t>(layout.subcatchCount));

    for (int32_t i = 0; i < layout.subcatchCount; ++i)
    {
        const int64_t position =
            layout.catchmentSummaryStartPos +
            static_cast<int64_t>(i) * valuesPerCatchmentSummary * static_cast<int64_t>(sizeof(double)) +
            catchmentSummaryAnnualImperviousIndex * static_cast<int64_t>(sizeof(double));

        reader.seek(position);

        SubcatchAnnualRunoffSummary summary;
        summary.imperviousRunoff = reader.read<double>();
        summary.perviousRunoff = reader.read<double>();
        summary.totalRunoff = reader.read<double>();
        summaries.push_back(summary);
    }

    return summaries;
}

std::vector<HydrologyNodeSummary> readHydrologyNodeSummaries(
    BinaryReader& reader,
    const RunoffBinaryLayout& layout)
{
    const int valuesPerHydrologyNodeSummary = 1 + layout.pollutantCount;

    std::vector<HydrologyNodeSummary> summaries;
    summaries.reserve(static_cast<size_t>(layout.hydrologyNodeCount));

    for (int32_t i = 0; i < layout.hydrologyNodeCount; ++i)
    {
        const int64_t position =
            layout.hydrologyNodeSummaryStartPos +
            static_cast<int64_t>(i) * valuesPerHydrologyNodeSummary * static_cast<int64_t>(sizeof(double));

        reader.seek(position);

        HydrologyNodeSummary summary;
        summary.totalInflowVolume = reader.read<double>();
        summaries.push_back(summary);
    }

    return summaries;
}

std::vector<HydrologyLinkSummary> readHydrologyLinkSummaries(
    BinaryReader& reader,
    const RunoffBinaryLayout& layout)
{
    const int valuesPerHydrologyLinkSummary = 2 + 2 * layout.pollutantCount;

    std::vector<HydrologyLinkSummary> summaries;
    summaries.reserve(static_cast<size_t>(layout.hydrologyLinkCount));

    for (int32_t i = 0; i < layout.hydrologyLinkCount; ++i)
    {
        const int64_t position =
            layout.hydrologyLinkSummaryStartPos +
            static_cast<int64_t>(i) * valuesPerHydrologyLinkSummary * static_cast<int64_t>(sizeof(double));

        reader.seek(position);

        HydrologyLinkSummary summary;
        summary.totalInflowVolume = reader.read<double>();
        summary.totalOutflowVolume = reader.read<double>();
        summaries.push_back(summary);
    }

    return summaries;
}

std::vector<HydrologyWSUDSummary> readHydrologyWSUDSummaries(
    BinaryReader& reader,
    const RunoffBinaryLayout& layout)
{
    const int valuesPerHydrologyWSUDSummary = 13 + 11 * layout.pollutantCount;

    std::vector<HydrologyWSUDSummary> summaries;
    summaries.reserve(static_cast<size_t>(layout.wsudCount));

    for (int32_t i = 0; i < layout.wsudCount; ++i)
    {
        const int64_t position =
            layout.hydrologyWSUDSummaryStartPos +
            static_cast<int64_t>(sizeof(double)) +
            static_cast<int64_t>(i) * valuesPerHydrologyWSUDSummary *
                static_cast<int64_t>(sizeof(double));

        reader.seek(position);

        HydrologyWSUDSummary summary;
        summary.volumeInflow = reader.read<double>();
        summary.volumeLostFromSystem = reader.read<double>();
        summary.volumeNetworkOutflow = reader.read<double>();
        summary.volumeWithdrawn = reader.read<double>();
        summary.volumeRequested = reader.read<double>();
        summary.volumeLowFlowBypass = reader.read<double>();
        summary.volumeHighFlowBypass = reader.read<double>();
        summary.volumeEvaporation = reader.read<double>();
        summary.volumeExfiltration = reader.read<double>();
        summary.volumeTreatedFlow = reader.read<double>();
        summary.volumeOverflow = reader.read<double>();
        summary.treatmentTrainSourceFlow = reader.read<double>();
        summary.treatmentTrainResidualFlow = reader.read<double>();
        summaries.push_back(summary);
    }

    return summaries;
}

RunoffBinaryResults readRunoffBinaryResults(const fs::path& runoffBinaryFile)
{
    RunoffBinaryResults results;
    BinaryReader reader(runoffBinaryFile);
    results.layout = readRunoffBinaryLayout(reader);

    BufferedTimeSeriesResults bufferedResults;
    readRunoffTimeSeries(reader, results.layout, bufferedResults);
    for (TimeSeriesResultRecord& record : bufferedResults.records)
    {
        if (record.type == RESULT_SUBCATCHMENT)
            results.subcatchmentRecords.push_back(std::move(record));
        else if (record.type == RESULT_HYDROLOGY_NODE)
            results.hydrologyNodeRecords.push_back(std::move(record));
        else if (record.type == RESULT_HYDROLOGY_LINK)
            results.hydrologyLinkRecords.push_back(std::move(record));
        else if (record.type == RESULT_HYDROLOGY_WSUD)
            results.hydrologyWSUDRecords.push_back(std::move(record));
    }

    results.subcatchmentSummaries = readSubcatchAnnualRunoffSummaries(reader, results.layout);
    results.hydrologyNodeSummaries = readHydrologyNodeSummaries(reader, results.layout);
    results.hydrologyLinkSummaries = readHydrologyLinkSummaries(reader, results.layout);
    results.hydrologyWSUDSummaries = readHydrologyWSUDSummaries(reader, results.layout);
    return results;
}

BufferedTimeSeriesResults populateTimeSeriesResults(
    const fs::path& runoffBinaryFile,
    const fs::path& routingBinaryFile)
{
    BufferedTimeSeriesResults results;

    BinaryReader runoffReader(runoffBinaryFile);
    const RunoffBinaryLayout runoffLayout = readRunoffBinaryLayout(runoffReader);
    readRunoffTimeSeries(runoffReader, runoffLayout, results);

    BinaryReader routingReader(routingBinaryFile);
    const RoutingBinaryLayout routingLayout = readRoutingBinaryLayout(routingReader);
    readRoutingTimeSeries(routingReader, routingLayout, results);

    return results;
}

std::string elementTypeName(int type)
{
    switch (type)
    {
    case RESULT_SUBCATCHMENT:
        return "subcatchment";
    case RESULT_HYDROLOGY_LINK:
        return "hydrology_link";
    case RESULT_HYDROLOGY_NODE:
        return "hydrology_node";
    case RESULT_HYDROLOGY_WSUD:
        return "hydrology_wsud";
    case RESULT_HYDRAULIC_NODE:
        return "hydraulic_node";
    case RESULT_HYDRAULIC_LINK:
        return "hydraulic_link";
    default:
        return "unknown";
    }
}

std::string selectedResultName(int type)
{
    switch (type)
    {
    case RESULT_SUBCATCHMENT:
        return "newRunoff";
    case RESULT_HYDROLOGY_LINK:
        return "currentFlow";
    case RESULT_HYDROLOGY_NODE:
        return "inflow";
    case RESULT_HYDROLOGY_WSUD:
    case RESULT_HYDRAULIC_NODE:
        return "depth";
    case RESULT_HYDRAULIC_LINK:
        return "flow";
    default:
        return "unknown";
    }
}

std::string secondaryResultName(int type)
{
    switch (type)
    {
    case RESULT_HYDRAULIC_NODE:
        return "inflow";
    default:
        return "";
    }
}

bool tryGetSelectedResultValue(
    const TimeSeriesResultRecord& record,
    const std::vector<float>& values,
    float& selectedValue)
{
    switch (record.type)
    {
    case RESULT_SUBCATCHMENT:
        // Subcatchment runoff is stored as pervious runoff + impervious runoff.
        if (values.size() <= 8) return false;
        selectedValue = values[4] + values[8];
        return true;

    case RESULT_HYDROLOGY_LINK:
        if (values.size() <= 1) return false;
        selectedValue = values[1];
        return true;

    case RESULT_HYDROLOGY_NODE:
    case RESULT_HYDROLOGY_WSUD:
    case RESULT_HYDRAULIC_NODE:
    case RESULT_HYDRAULIC_LINK:
        if (values.empty()) return false;
        selectedValue = values[0];
        return true;

    default:
        return false;
    }
}

bool tryGetSecondaryResultValue(
    const TimeSeriesResultRecord& record,
    const std::vector<float>& values,
    float& secondaryValue)
{
    switch (record.type)
    {
    case RESULT_HYDRAULIC_NODE:
        if (values.size() <= 4) return false;
        secondaryValue = values[4];
        return true;

    default:
        return false;
    }
}

fs::path findBinaryFile(
    const fs::path& baseOutputFolder,
    const fs::path& modelFolder,
    const std::string& filename)
{
    const fs::path nestedPath = modelFolder / filename;
    if (fs::exists(nestedPath))
        return nestedPath;

    const fs::path flatPath = baseOutputFolder / filename;
    if (fs::exists(flatPath))
        return flatPath;

    return nestedPath;
}

void writeSelectedTimeSeriesResults(
    std::ostream& output,
    const std::string& modelName,
    const BufferedTimeSeriesResults& results)
{
    output << "MODEL\t" << modelName << "\n";

    for (const TimeSeriesResultRecord& record : results.records)
    {
        const std::string secondaryName = secondaryResultName(record.type);
        output << "ELEMENT\t" << elementTypeName(record.type)
            << "\t" << record.id
            << "\t" << selectedResultName(record.type);
        if (!secondaryName.empty())
            output << "\t" << secondaryName;
        output << "\n";

        output << "date_time\tvalue";
        if (!secondaryName.empty())
            output << "\tsecondary_value";
        output << "\n";

        for (size_t i = 0; i < record.values.size(); ++i)
        {
            float selectedValue = 0.0f;
            if (!tryGetSelectedResultValue(record, record.values[i], selectedValue))
            {
                output << formatDateTime(record.times[i]) << "\t<missing>\n";
                continue;
            }

            output << formatDateTime(record.times[i]) << "\t" << selectedValue;
            if (!secondaryName.empty())
            {
                float secondaryValue = 0.0f;
                if (tryGetSecondaryResultValue(record, record.values[i], secondaryValue))
                    output << "\t" << secondaryValue;
                else
                    output << "\t<missing>";
            }
            output << "\n";
        }

        output << "\n";
    }

    output << "\n";
}

int exportTemporaryBinaryTimeSeriesResults(const fs::path& baseOutputFolder)
{
    const fs::path outputFile = baseOutputFolder / "temporary_binary_timeseries_results.txt";
    std::ofstream output(outputFile);
    if (!output)
    {
        std::cerr << "WARNING: Could not create temporary time-series output file: "
            << outputFile << "\n";
        return 1;
    }

    int errors = 0;

    for (const auto& entry : fs::directory_iterator(baseOutputFolder))
    {
        if (!entry.is_directory())
            continue;

        const fs::path modelFolder = entry.path();
        const std::string modelName = modelFolder.filename().string();
        const fs::path runoffBinary =
            findBinaryFile(baseOutputFolder, modelFolder, modelName + "_runoff.bin");
        const fs::path routingBinary =
            findBinaryFile(baseOutputFolder, modelFolder, modelName + ".bin");

        if (!fs::exists(runoffBinary))
        {
            std::cerr << "WARNING: Runoff binary not found for model '" << modelName
                << "': " << runoffBinary << "\n";
            ++errors;
            continue;
        }

        if (!fs::exists(routingBinary))
        {
            std::cerr << "WARNING: Routing binary not found for model '" << modelName
                << "': " << routingBinary << "\n";
            ++errors;
            continue;
        }

        try
        {
            const BufferedTimeSeriesResults results =
                populateTimeSeriesResults(runoffBinary, routingBinary);
            writeSelectedTimeSeriesResults(output, modelName, results);
        }
        catch (const std::exception& e)
        {
            std::cerr << "WARNING: Could not read binary time-series results for model '"
                << modelName << "': " << e.what() << "\n";
            ++errors;
        }
    }

    std::cout << "Temporary binary time-series output: " << outputFile << "\n";
    return errors;
}

struct SubcatchVariableComparisonSpec
{
    int index = 0;
    const char* name = "";
    ToleranceQuantity quantity = ToleranceQuantity::Flow;
    bool requireExactMatch = false;
};

const std::vector<SubcatchVariableComparisonSpec>& subcatchVariableComparisonSpecs()
{
    static const std::vector<SubcatchVariableComparisonSpec> specs =
    {
        { 0, "SUBCATCH_RAINFALL", ToleranceQuantity::Flow, true },
        { 1, "SUBCATCH_SNOWDEPTH/SURM_BASEFLOW", ToleranceQuantity::Depth, false },
        { 2, "SUBCATCH_EVAP", ToleranceQuantity::Flow, false },
        { 3, "SUBCATCH_INFIL", ToleranceQuantity::Flow, false },
        { 4, "SUBCATCH_PERVIOUS_RUNOFF", ToleranceQuantity::Flow, false },
        { 5, "SUBCATCH_GW_FLOW", ToleranceQuantity::Flow, false },
        { 6, "SUBCATCH_GW_ELEV", ToleranceQuantity::Depth, false },
        { 7, "SUBCATCH_SOIL_MOIST", ToleranceQuantity::PercentPoint, false },
        { 8, "SUBCATCH_IMPERVIOUS_RUNOFF", ToleranceQuantity::Flow, false }
    };

    return specs;
}

std::map<std::string, size_t> buildRecordIndex(
    const std::vector<TimeSeriesResultRecord>& records)
{
    std::map<std::string, size_t> index;

    for (size_t i = 0; i < records.size(); ++i)
        index[records[i].id] = i;

    return index;
}

std::string formatDeviation(double deviationPercent)
{
    if (std::isinf(deviationPercent))
        return "INF";

    std::ostringstream output;
    output << deviationPercent;
    return output.str();
}

enum class ComparisonStatus
{
    Pass,
    Warn,
    Fail
};

const char* comparisonStatusText(ComparisonStatus status)
{
    switch (status)
    {
    case ComparisonStatus::Pass:
        return "PASS";
    case ComparisonStatus::Warn:
        return "WARN";
    case ComparisonStatus::Fail:
        return "FAIL";
    default:
        return "FAIL";
    }
}

void combineStatus(ComparisonStatus& aggregateStatus, ComparisonStatus newStatus)
{
    if (newStatus == ComparisonStatus::Fail)
        aggregateStatus = ComparisonStatus::Fail;
    else if (newStatus == ComparisonStatus::Warn && aggregateStatus == ComparisonStatus::Pass)
        aggregateStatus = ComparisonStatus::Warn;
}

int comparisonStatusRank(ComparisonStatus status)
{
    switch (status)
    {
    case ComparisonStatus::Pass:
        return 0;
    case ComparisonStatus::Warn:
        return 1;
    case ComparisonStatus::Fail:
        return 2;
    default:
        return 2;
    }
}

double percentageDeviation(double originalValue, double newValue)
{
    if (originalValue == 0.0)
        return newValue == 0.0 ? 0.0 : std::numeric_limits<double>::infinity();

    return std::fabs((newValue - originalValue) / originalValue) * 100.0;
}

struct ValueComparisonResult
{
    ComparisonStatus status = ComparisonStatus::Pass;
    double percentDeviation = 0.0;
    double absoluteDifference = 0.0;
    bool usedAbsolute = false;
    double checkedDifference = 0.0;
};

ValueComparisonResult compareValuesWithTolerance(
    double originalValue,
    double newValue,
    const ToleranceRule& rule)
{
    ValueComparisonResult result;
    result.percentDeviation = percentageDeviation(originalValue, newValue);
    result.absoluteDifference = std::fabs(newValue - originalValue);
    result.usedAbsolute =
        rule.useAbsoluteAlways ||
        (rule.useAbsoluteBelowNearZero &&
            (std::fabs(originalValue) < rule.nearZeroThreshold ||
                std::fabs(newValue) < rule.nearZeroThreshold));

    if (result.usedAbsolute)
    {
        result.checkedDifference = result.absoluteDifference;
        if (result.checkedDifference > rule.failAboveAbsolute)
            result.status = ComparisonStatus::Fail;
        else if (result.checkedDifference > rule.warnAboveAbsolute)
            result.status = ComparisonStatus::Warn;
    }
    else
    {
        result.checkedDifference = result.percentDeviation;
        if (result.checkedDifference > rule.failAbovePercent)
            result.status = ComparisonStatus::Fail;
        else if (result.checkedDifference > rule.warnAbovePercent)
            result.status = ComparisonStatus::Warn;
    }

    return result;
}

void addFailReason(std::vector<std::string>& failReasons, const std::string& reason)
{
    if (std::find(failReasons.begin(), failReasons.end(), reason) == failReasons.end())
        failReasons.push_back(reason);
}

void writeDeviation(std::ostream& output, double deviationPercent)
{
    output << formatDeviation(deviationPercent);
}

std::string trim(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
        ++first;

    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
        --last;

    return text.substr(first, last - first);
}

std::string toLower(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

bool parseBoolValue(const std::string& value)
{
    const std::string lowerValue = toLower(trim(value));
    return lowerValue == "true" || lowerValue == "1" || lowerValue == "yes";
}

ToleranceRule* toleranceRuleForSection(ToleranceSettings& settings, const std::string& section)
{
    const std::string lowerSection = toLower(section);
    if (lowerSection == "flow") return &settings.flow;
    if (lowerSection == "volume") return &settings.volume;
    if (lowerSection == "depth") return &settings.depth;
    if (lowerSection == "water_surface_profile") return &settings.waterSurfaceProfile;
    if (lowerSection == "velocity") return &settings.velocity;
    if (lowerSection == "continuity_error") return &settings.continuityError;
    if (lowerSection == "time") return &settings.time;
    if (lowerSection == "count") return &settings.count;
    if (lowerSection == "percent_point") return &settings.percentPoint;
    if (lowerSection == "flow_regime") return &settings.flowRegime;
    return nullptr;
}

const ToleranceRule* toleranceRuleForSection(
    const ToleranceSettings& settings,
    const std::string& section)
{
    const std::string lowerSection = toLower(section);
    if (lowerSection == "flow") return &settings.flow;
    if (lowerSection == "volume") return &settings.volume;
    if (lowerSection == "depth") return &settings.depth;
    if (lowerSection == "water_surface_profile") return &settings.waterSurfaceProfile;
    if (lowerSection == "velocity") return &settings.velocity;
    if (lowerSection == "continuity_error") return &settings.continuityError;
    if (lowerSection == "time") return &settings.time;
    if (lowerSection == "count") return &settings.count;
    if (lowerSection == "percent_point") return &settings.percentPoint;
    if (lowerSection == "flow_regime") return &settings.flowRegime;
    return nullptr;
}

double toleranceRuleDoubleValue(const ToleranceRule& rule, const std::string& key)
{
    if (key == "warn_above_percent") return rule.warnAbovePercent;
    if (key == "fail_above_percent") return rule.failAbovePercent;
    if (key == "warn_above_absolute") return rule.warnAboveAbsolute;
    if (key == "fail_above_absolute") return rule.failAboveAbsolute;
    if (key == "near_zero_threshold") return rule.nearZeroThreshold;
    throw std::runtime_error("Unknown numeric tolerance key: " + key);
}

bool toleranceRuleBoolValue(const ToleranceRule& rule, const std::string& key)
{
    if (key == "use_absolute_always") return rule.useAbsoluteAlways;
    if (key == "use_absolute_below_near_zero") return rule.useAbsoluteBelowNearZero;
    throw std::runtime_error("Unknown boolean tolerance key: " + key);
}

void setToleranceRuleValue(ToleranceRule& rule, const std::string& key, const std::string& value)
{
    if (key == "warn_above_percent")
        rule.warnAbovePercent = std::stod(value);
    else if (key == "fail_above_percent")
        rule.failAbovePercent = std::stod(value);
    else if (key == "warn_above_absolute")
        rule.warnAboveAbsolute = std::stod(value);
    else if (key == "fail_above_absolute")
        rule.failAboveAbsolute = std::stod(value);
    else if (key == "near_zero_threshold")
        rule.nearZeroThreshold = std::stod(value);
    else if (key == "use_absolute_always")
        rule.useAbsoluteAlways = parseBoolValue(value);
    else if (key == "use_absolute_below_near_zero")
        rule.useAbsoluteBelowNearZero = parseBoolValue(value);
    else
        throw std::runtime_error("Unknown tolerance key: " + key);
}

bool doublesDiffer(double a, double b)
{
    return std::fabs(a - b) > 1.0e-12;
}

void addToleranceOverrideMessage(
    std::vector<std::string>& messages,
    const std::string& section,
    const std::string& key,
    const std::string& value)
{
    messages.push_back(
        "Global setting for " + section + "/" + key +
        " was overwritten by the ini file, the new setting is: " + value);
}

ToleranceSettings readToleranceSettingsForModel(
    const fs::path& modelFolder,
    std::vector<std::string>& overrideMessages)
{
    ToleranceSettings settings = defaultToleranceSettings();
    const ToleranceSettings globalSettings = defaultToleranceSettings();
    const fs::path toleranceIniPath = modelFolder / "tolerance.ini";
    if (!fs::exists(toleranceIniPath))
        return settings;

    std::ifstream input(toleranceIniPath);
    if (!input)
        throw std::runtime_error("Could not open tolerance ini file: " + toleranceIniPath.string());

    std::string currentSection;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        const size_t commentPosition = line.find_first_of("#;");
        if (commentPosition != std::string::npos)
            line = line.substr(0, commentPosition);

        line = trim(line);
        if (line.empty())
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            currentSection = toLower(trim(line.substr(1, line.size() - 2)));
            if (!toleranceRuleForSection(settings, currentSection))
                throw std::runtime_error(
                    "Unknown tolerance section in " + toleranceIniPath.string() +
                    " at line " + std::to_string(lineNumber) + ": " + currentSection);
            continue;
        }

        const size_t equalsPosition = line.find('=');
        if (equalsPosition == std::string::npos || currentSection.empty())
            throw std::runtime_error(
                "Invalid tolerance ini line in " + toleranceIniPath.string() +
                " at line " + std::to_string(lineNumber));

        const std::string key = toLower(trim(line.substr(0, equalsPosition)));
        const std::string value = trim(line.substr(equalsPosition + 1));
        ToleranceRule* rule = toleranceRuleForSection(settings, currentSection);
        const ToleranceRule* globalRule = toleranceRuleForSection(globalSettings, currentSection);
        if (!rule || !globalRule)
            throw std::runtime_error("Invalid tolerance section: " + currentSection);

        setToleranceRuleValue(*rule, key, value);

        if (key == "use_absolute_always" || key == "use_absolute_below_near_zero")
        {
            if (toleranceRuleBoolValue(*rule, key) != toleranceRuleBoolValue(*globalRule, key))
                addToleranceOverrideMessage(overrideMessages, currentSection, key, value);
        }
        else
        {
            if (doublesDiffer(toleranceRuleDoubleValue(*rule, key), toleranceRuleDoubleValue(*globalRule, key)))
                addToleranceOverrideMessage(overrideMessages, currentSection, key, value);
        }
    }

    return settings;
}

const char* toleranceQuantityName(ToleranceQuantity quantity)
{
    switch (quantity)
    {
    case ToleranceQuantity::Flow:
        return "flow";
    case ToleranceQuantity::Volume:
        return "volume";
    case ToleranceQuantity::Depth:
        return "depth";
    case ToleranceQuantity::WaterSurfaceProfile:
        return "water_surface_profile";
    case ToleranceQuantity::Velocity:
        return "velocity";
    case ToleranceQuantity::ContinuityError:
        return "continuity_error";
    case ToleranceQuantity::Time:
        return "time";
    case ToleranceQuantity::Count:
        return "count";
    case ToleranceQuantity::PercentPoint:
        return "percent_point";
    case ToleranceQuantity::FlowRegime:
        return "flow_regime";
    default:
        return "unknown";
    }
}

void writeToleranceRule(
    std::ostream& output,
    const ToleranceSettings& settings,
    ToleranceQuantity quantity)
{
    const ToleranceRule rule = toleranceRule(settings, quantity);
    output << "  "
        << std::left << std::setw(24) << toleranceQuantityName(quantity)
        << "warn_percent=" << std::setw(10) << rule.warnAbovePercent
        << "fail_percent=" << std::setw(10) << rule.failAbovePercent
        << "warn_absolute=" << std::setw(12) << rule.warnAboveAbsolute
        << "fail_absolute=" << std::setw(12) << rule.failAboveAbsolute
        << "near_zero=" << std::setw(12) << rule.nearZeroThreshold
        << "absolute_always=" << std::setw(6) << (rule.useAbsoluteAlways ? "true" : "false")
        << "absolute_below_near_zero=" << (rule.useAbsoluteBelowNearZero ? "true" : "false")
        << "\n";
}

void writeToleranceOverview(
    std::ostream& output,
    const ToleranceSettings& settings,
    const std::vector<std::string>& overrideMessages)
{
    output << "Tolerance settings used\n";
    output << "-----------------------\n";
    writeToleranceRule(output, settings, ToleranceQuantity::Flow);
    writeToleranceRule(output, settings, ToleranceQuantity::Volume);
    writeToleranceRule(output, settings, ToleranceQuantity::Depth);
    writeToleranceRule(output, settings, ToleranceQuantity::WaterSurfaceProfile);
    writeToleranceRule(output, settings, ToleranceQuantity::Velocity);
    writeToleranceRule(output, settings, ToleranceQuantity::ContinuityError);
    writeToleranceRule(output, settings, ToleranceQuantity::Time);
    writeToleranceRule(output, settings, ToleranceQuantity::Count);
    writeToleranceRule(output, settings, ToleranceQuantity::PercentPoint);
    writeToleranceRule(output, settings, ToleranceQuantity::FlowRegime);
    if (!overrideMessages.empty())
    {
        output << "\nTolerance overrides\n";
        output << "-------------------\n";
        for (const std::string& message : overrideMessages)
            output << message << "\n";
    }
    output << "\n";
}

void writeTimeSeriesComparisonRow(
    std::ostream& detailReport,
    const std::string& name,
    ComparisonStatus status,
    int warnedValues,
    int failedValues,
    int missingValues,
    double maxCheckedDifference,
    double maxPercentDeviation,
    double maxAbsoluteDifference,
    const std::string& maxDifferenceMode,
    const std::string& maxDeviationTime,
    double originalValueAtMaxDeviation,
    double newValueAtMaxDeviation)
{
    detailReport << "  "
        << std::left << std::setw(36) << name
        << std::setw(8) << comparisonStatusText(status)
        << "warned_values=" << std::right << std::setw(8) << warnedValues << "  "
        << "failed_values=" << std::right << std::setw(8) << failedValues << "  "
        << "missing_values=" << std::right << std::setw(8) << missingValues << "  "
        << "max_checked_difference=" << std::right << std::setw(14) << formatDeviation(maxCheckedDifference) << "  "
        << "mode=" << std::left << std::setw(8) << maxDifferenceMode
        << "max_deviation_percent=" << std::right << std::setw(14) << formatDeviation(maxPercentDeviation) << "  "
        << "max_absolute_difference=" << std::right << std::setw(14) << maxAbsoluteDifference << "  "
        << "at=" << maxDeviationTime << "  "
        << "original_at_max=" << std::setw(14) << originalValueAtMaxDeviation << "  "
        << "new_at_max=" << std::setw(14) << newValueAtMaxDeviation << "\n"
        << std::left;
}

struct TimeSeriesVariableComparisonSpec
{
    int index = 0;
    const char* name = "";
    ToleranceQuantity quantity = ToleranceQuantity::Flow;
};

ComparisonStatus compareTimeSeriesVariables(
    const std::string& sectionTitle,
    const TimeSeriesResultRecord& originalRecord,
    const TimeSeriesResultRecord& newRecord,
    const std::vector<TimeSeriesVariableComparisonSpec>& specs,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const std::string& elementTypeForReason,
    const ToleranceSettings& toleranceSettings)
{
    ComparisonStatus aggregateStatus = ComparisonStatus::Pass;

    detailReport << "\n" << sectionTitle << ": " << originalRecord.id << "\n";

    for (const TimeSeriesVariableComparisonSpec& spec : specs)
    {
        ComparisonStatus variableStatus = ComparisonStatus::Pass;
        int warnedValues = 0;
        int failedValues = 0;
        int missingValues = 0;
        double maxCheckedDifference = 0.0;
        double maxPercentDeviation = 0.0;
        double maxAbsoluteDifference = 0.0;
        size_t maxDeviationPeriod = 0;
        double originalValueAtMaxDeviation = 0.0;
        double newValueAtMaxDeviation = 0.0;
        std::string maxDifferenceMode = "percent";
        ComparisonStatus selectedDisplayStatus = ComparisonStatus::Pass;
        bool hasDisplayValue = false;
        const ToleranceRule rule = toleranceRule(toleranceSettings, spec.quantity);

        for (size_t period = 0; period < originalRecord.values.size(); ++period)
        {
            const std::vector<float>& originalValues = originalRecord.values[period];
            const std::vector<float>& newValues = newRecord.values[period];

            if (originalValues.size() <= static_cast<size_t>(spec.index) ||
                newValues.size() <= static_cast<size_t>(spec.index))
            {
                variableStatus = ComparisonStatus::Fail;
                ++failedValues;
                ++missingValues;
                continue;
            }

            const double originalValue = originalValues[spec.index];
            const double newValue = newValues[spec.index];
            const ValueComparisonResult valueResult =
                compareValuesWithTolerance(originalValue, newValue, rule);

            const bool shouldUseAsDisplayValue =
                !hasDisplayValue ||
                comparisonStatusRank(valueResult.status) > comparisonStatusRank(selectedDisplayStatus) ||
                (valueResult.status == selectedDisplayStatus &&
                    valueResult.checkedDifference > maxCheckedDifference);

            if (shouldUseAsDisplayValue)
            {
                hasDisplayValue = true;
                selectedDisplayStatus = valueResult.status;
                maxCheckedDifference = valueResult.checkedDifference;
                maxPercentDeviation = valueResult.percentDeviation;
                maxAbsoluteDifference = valueResult.absoluteDifference;
                maxDeviationPeriod = period;
                originalValueAtMaxDeviation = originalValue;
                newValueAtMaxDeviation = newValue;
                maxDifferenceMode = valueResult.usedAbsolute ? "absolute" : "percent";
            }

            if (valueResult.status == ComparisonStatus::Fail)
            {
                ++failedValues;
            }
            else if (valueResult.status == ComparisonStatus::Warn)
            {
                ++warnedValues;
            }

            combineStatus(variableStatus, valueResult.status);
        }

        combineStatus(aggregateStatus, variableStatus);

        if (variableStatus == ComparisonStatus::Fail)
        {
            addFailReason(
                failReasons,
                "FAIL time-step " + elementTypeForReason + " " + spec.name);
        }

        const std::string maxDeviationTime =
            (!originalRecord.times.empty() && maxDeviationPeriod < originalRecord.times.size()) ?
            formatDateTime(originalRecord.times[maxDeviationPeriod]) : "<missing>";
        writeTimeSeriesComparisonRow(
            detailReport,
            spec.name,
            variableStatus,
            warnedValues,
            failedValues,
            missingValues,
            maxCheckedDifference,
            maxPercentDeviation,
            maxAbsoluteDifference,
            maxDifferenceMode,
            maxDeviationTime,
            originalValueAtMaxDeviation,
            newValueAtMaxDeviation);
    }

    return aggregateStatus;
}

ComparisonStatus compareSubcatchmentTimeSeries(
    const TimeSeriesResultRecord& originalRecord,
    const TimeSeriesResultRecord& newRecord,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    ComparisonStatus aggregateStatus = ComparisonStatus::Pass;

    detailReport << "\nSubcatchment: " << originalRecord.id << "\n";

    for (const SubcatchVariableComparisonSpec& spec : subcatchVariableComparisonSpecs())
    {
        ComparisonStatus variableStatus = ComparisonStatus::Pass;
        int warnedValues = 0;
        int failedValues = 0;
        int missingValues = 0;
        double maxCheckedDifference = 0.0;
        double maxPercentDeviation = 0.0;
        double maxAbsoluteDifference = 0.0;
        size_t maxDeviationPeriod = 0;
        double originalValueAtMaxDeviation = 0.0;
        double newValueAtMaxDeviation = 0.0;
        std::string maxDifferenceMode = spec.requireExactMatch ? "exact" : "percent";
        ComparisonStatus selectedDisplayStatus = ComparisonStatus::Pass;
        bool hasDisplayValue = false;
        const ToleranceRule rule = toleranceRule(toleranceSettings, spec.quantity);

        for (size_t period = 0; period < originalRecord.values.size(); ++period)
        {
            const std::vector<float>& originalValues = originalRecord.values[period];
            const std::vector<float>& newValues = newRecord.values[period];

            if (originalValues.size() <= static_cast<size_t>(spec.index) ||
                newValues.size() <= static_cast<size_t>(spec.index))
            {
                variableStatus = ComparisonStatus::Fail;
                ++failedValues;
                ++missingValues;
                continue;
            }

            const double originalValue = originalValues[spec.index];
            const double newValue = newValues[spec.index];
            ValueComparisonResult valueResult;

            if (spec.requireExactMatch)
            {
                valueResult.absoluteDifference = std::fabs(newValue - originalValue);
                valueResult.percentDeviation = percentageDeviation(originalValue, newValue);
                valueResult.checkedDifference = valueResult.absoluteDifference;
                valueResult.usedAbsolute = true;
                valueResult.status = originalValue == newValue ?
                    ComparisonStatus::Pass : ComparisonStatus::Fail;
            }
            else
            {
                valueResult = compareValuesWithTolerance(originalValue, newValue, rule);
            }

            const bool shouldUseAsDisplayValue =
                !hasDisplayValue ||
                comparisonStatusRank(valueResult.status) > comparisonStatusRank(selectedDisplayStatus) ||
                (valueResult.status == selectedDisplayStatus &&
                    valueResult.checkedDifference > maxCheckedDifference);

            if (shouldUseAsDisplayValue)
            {
                hasDisplayValue = true;
                selectedDisplayStatus = valueResult.status;
                maxCheckedDifference = valueResult.checkedDifference;
                maxPercentDeviation = valueResult.percentDeviation;
                maxAbsoluteDifference = valueResult.absoluteDifference;
                maxDeviationPeriod = period;
                originalValueAtMaxDeviation = originalValue;
                newValueAtMaxDeviation = newValue;
                maxDifferenceMode = valueResult.usedAbsolute ? "absolute" : "percent";
            }

            if (valueResult.status == ComparisonStatus::Fail)
            {
                ++failedValues;
            }
            else if (valueResult.status == ComparisonStatus::Warn)
            {
                ++warnedValues;
            }

            combineStatus(variableStatus, valueResult.status);
        }

        combineStatus(aggregateStatus, variableStatus);

        if (variableStatus == ComparisonStatus::Fail)
        {
            addFailReason(
                failReasons,
                std::string("FAIL time-step subcatchment ") + spec.name);
        }

        const std::string maxDeviationTime =
            (!originalRecord.times.empty() && maxDeviationPeriod < originalRecord.times.size()) ?
            formatDateTime(originalRecord.times[maxDeviationPeriod]) : "<missing>";
        writeTimeSeriesComparisonRow(
            detailReport,
            spec.name,
            variableStatus,
            warnedValues,
            failedValues,
            missingValues,
            maxCheckedDifference,
            maxPercentDeviation,
            maxAbsoluteDifference,
            maxDifferenceMode,
            maxDeviationTime,
            originalValueAtMaxDeviation,
            newValueAtMaxDeviation);
    }

    return aggregateStatus;
}

struct SummaryComparisonSpec
{
    const char* name;
    double originalValue;
    double newValue;
    ToleranceQuantity quantity;
};

ComparisonStatus compareSubcatchmentSummaries(
    const std::string& id,
    const SubcatchAnnualRunoffSummary& originalSummary,
    const SubcatchAnnualRunoffSummary& newSummary,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    const SummaryComparisonSpec specs[] =
    {
        { "annual_impervious_runoff", originalSummary.imperviousRunoff, newSummary.imperviousRunoff, ToleranceQuantity::Volume },
        { "annual_pervious_runoff", originalSummary.perviousRunoff, newSummary.perviousRunoff, ToleranceQuantity::Volume },
        { "annual_total_runoff", originalSummary.totalRunoff, newSummary.totalRunoff, ToleranceQuantity::Volume }
    };

    ComparisonStatus aggregateStatus = ComparisonStatus::Pass;

    detailReport << "\nSubcatchment summary: " << id << "\n";
    for (const SummaryComparisonSpec& spec : specs)
    {
        const ValueComparisonResult valueResult =
            compareValuesWithTolerance(
                spec.originalValue,
                spec.newValue,
                toleranceRule(toleranceSettings, spec.quantity));
        combineStatus(aggregateStatus, valueResult.status);
        if (valueResult.status == ComparisonStatus::Fail)
        {
            addFailReason(
                failReasons,
                std::string("FAIL summary subcatchment ") + spec.name);
        }

        detailReport << "  "
            << std::left << std::setw(36) << spec.name
            << std::setw(8) << comparisonStatusText(valueResult.status)
            << "original=" << std::setw(14) << spec.originalValue
            << "new=" << std::setw(14) << spec.newValue
            << "checked_difference=" << std::setw(14) << formatDeviation(valueResult.checkedDifference)
            << "mode=" << std::setw(10) << (valueResult.usedAbsolute ? "absolute" : "percent")
            << "deviation_percent=" << std::setw(14) << formatDeviation(valueResult.percentDeviation)
            << "absolute_difference=" << std::setw(14) << valueResult.absoluteDifference
            << "\n";
    }

    return aggregateStatus;
}

template <typename Summary>
ComparisonStatus compareSummaryValues(
    const std::string& sectionTitle,
    const std::string& id,
    const Summary& originalSummary,
    const Summary& newSummary,
    const std::vector<SummaryComparisonSpec>& values,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const std::string& elementTypeForReason,
    const ToleranceSettings& toleranceSettings)
{
    ComparisonStatus aggregateStatus = ComparisonStatus::Pass;

    detailReport << "\n" << sectionTitle << ": " << id << "\n";
    for (const SummaryComparisonSpec& value : values)
    {
        const ValueComparisonResult valueResult =
            compareValuesWithTolerance(
                value.originalValue,
                value.newValue,
                toleranceRule(toleranceSettings, value.quantity));
        combineStatus(aggregateStatus, valueResult.status);
        if (valueResult.status == ComparisonStatus::Fail)
        {
            addFailReason(
                failReasons,
                "FAIL summary " + elementTypeForReason + " " + value.name);
        }

        detailReport << "  "
            << std::left << std::setw(36) << value.name
            << std::setw(8) << comparisonStatusText(valueResult.status)
            << "original=" << std::setw(14) << value.originalValue
            << "new=" << std::setw(14) << value.newValue
            << "checked_difference=" << std::setw(14) << formatDeviation(valueResult.checkedDifference)
            << "mode=" << std::setw(10) << (valueResult.usedAbsolute ? "absolute" : "percent")
            << "deviation_percent=" << std::setw(14) << formatDeviation(valueResult.percentDeviation)
            << "absolute_difference=" << std::setw(14) << valueResult.absoluteDifference
            << "\n";
    }

    return aggregateStatus;
}

ComparisonStatus compareHydrologyNodeSummary(
    const std::string& id,
    const HydrologyNodeSummary& originalSummary,
    const HydrologyNodeSummary& newSummary,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    return compareSummaryValues(
        "Hydrology node summary",
        id,
        originalSummary,
        newSummary,
        { { "total_inflow_volume", originalSummary.totalInflowVolume, newSummary.totalInflowVolume, ToleranceQuantity::Volume } },
        detailReport,
        failReasons,
        "hydrology node",
        toleranceSettings);
}

ComparisonStatus compareHydrologyLinkSummary(
    const std::string& id,
    const HydrologyLinkSummary& originalSummary,
    const HydrologyLinkSummary& newSummary,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    return compareSummaryValues(
        "Hydrology link summary",
        id,
        originalSummary,
        newSummary,
        {
            { "total_inflow_volume", originalSummary.totalInflowVolume, newSummary.totalInflowVolume, ToleranceQuantity::Volume },
            { "total_outflow_volume", originalSummary.totalOutflowVolume, newSummary.totalOutflowVolume, ToleranceQuantity::Volume }
        },
        detailReport,
        failReasons,
        "hydrology link",
        toleranceSettings);
}

ComparisonStatus compareHydrologyWSUDSummary(
    const std::string& id,
    const HydrologyWSUDSummary& originalSummary,
    const HydrologyWSUDSummary& newSummary,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    return compareSummaryValues(
        "Hydrology WSUD summary",
        id,
        originalSummary,
        newSummary,
        {
            { "volume_inflow", originalSummary.volumeInflow, newSummary.volumeInflow, ToleranceQuantity::Volume },
            { "volume_lost_from_system", originalSummary.volumeLostFromSystem, newSummary.volumeLostFromSystem, ToleranceQuantity::Volume },
            { "volume_network_outflow", originalSummary.volumeNetworkOutflow, newSummary.volumeNetworkOutflow, ToleranceQuantity::Volume },
            { "volume_withdrawn", originalSummary.volumeWithdrawn, newSummary.volumeWithdrawn, ToleranceQuantity::Volume },
            { "volume_requested", originalSummary.volumeRequested, newSummary.volumeRequested, ToleranceQuantity::Volume },
            { "volume_low_flow_bypass", originalSummary.volumeLowFlowBypass, newSummary.volumeLowFlowBypass, ToleranceQuantity::Volume },
            { "volume_high_flow_bypass", originalSummary.volumeHighFlowBypass, newSummary.volumeHighFlowBypass, ToleranceQuantity::Volume },
            { "volume_evaporation", originalSummary.volumeEvaporation, newSummary.volumeEvaporation, ToleranceQuantity::Volume },
            { "volume_exfiltration", originalSummary.volumeExfiltration, newSummary.volumeExfiltration, ToleranceQuantity::Volume },
            { "volume_treated_flow", originalSummary.volumeTreatedFlow, newSummary.volumeTreatedFlow, ToleranceQuantity::Volume },
            { "volume_overflow", originalSummary.volumeOverflow, newSummary.volumeOverflow, ToleranceQuantity::Volume },
            { "treatment_train_source_flow", originalSummary.treatmentTrainSourceFlow, newSummary.treatmentTrainSourceFlow, ToleranceQuantity::Volume },
            { "treatment_train_residual_flow", originalSummary.treatmentTrainResidualFlow, newSummary.treatmentTrainResidualFlow, ToleranceQuantity::Volume }
        },
        detailReport,
        failReasons,
        "hydrology WSUD",
        toleranceSettings);
}

ComparisonStatus compareHydraulicNodeSummary(
    const std::string& id,
    const HydraulicNodeSummary& originalSummary,
    const HydraulicNodeSummary& newSummary,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    return compareSummaryValues(
        "Hydraulic node summary",
        id,
        originalSummary,
        newSummary,
        {
            { "avg_depth", originalSummary.avgDepth, newSummary.avgDepth, ToleranceQuantity::Depth },
            { "max_depth", originalSummary.maxDepth, newSummary.maxDepth, ToleranceQuantity::Depth },
            { "max_lateral_flow", originalSummary.maxLatFlow, newSummary.maxLatFlow, ToleranceQuantity::Flow },
            { "max_inflow", originalSummary.maxInflow, newSummary.maxInflow, ToleranceQuantity::Flow },
            { "total_lateral_flow", originalSummary.totalLatFlow, newSummary.totalLatFlow, ToleranceQuantity::Volume },
            { "total_inflow", originalSummary.totalInflow, newSummary.totalInflow, ToleranceQuantity::Volume },
            { "continuity_error", originalSummary.continuityError, newSummary.continuityError, ToleranceQuantity::ContinuityError },
            { "volume_flooded", originalSummary.volumeFlooded, newSummary.volumeFlooded, ToleranceQuantity::Volume }
        },
        detailReport,
        failReasons,
        "hydraulic node",
        toleranceSettings);
}

ComparisonStatus compareHydraulicLinkSummary(
    const std::string& id,
    const HydraulicLinkSummary& originalSummary,
    const HydraulicLinkSummary& newSummary,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    return compareSummaryValues(
        "Hydraulic link summary",
        id,
        originalSummary,
        newSummary,
        {
            { "max_flow", originalSummary.maxFlow, newSummary.maxFlow, ToleranceQuantity::Flow },
            { "max_velocity", originalSummary.maxVelocity, newSummary.maxVelocity, ToleranceQuantity::Velocity },
            { "max_flow_ratio", originalSummary.maxFlowRatio, newSummary.maxFlowRatio, ToleranceQuantity::PercentPoint }
        },
        detailReport,
        failReasons,
        "hydraulic link",
        toleranceSettings);
}

ComparisonStatus compareRecordCollections(
    const std::string& elementTypeName,
    const std::string& sectionTitle,
    const std::vector<TimeSeriesResultRecord>& originalRecords,
    const std::vector<TimeSeriesResultRecord>& newRecords,
    const std::vector<TimeSeriesVariableComparisonSpec>& specs,
    bool periodsMatch,
    std::ostream& detailReport,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings)
{
    ComparisonStatus aggregateStatus = ComparisonStatus::Pass;

    detailReport << "\nOriginal " << elementTypeName << " count: "
        << originalRecords.size() << "\n";
    for (const TimeSeriesResultRecord& record : originalRecords)
        detailReport << "  original ID: " << record.id << "\n";

    detailReport << "\nNew " << elementTypeName << " count: "
        << newRecords.size() << "\n";
    for (const TimeSeriesResultRecord& record : newRecords)
        detailReport << "  new ID: " << record.id << "\n";

    if (originalRecords.size() != newRecords.size())
    {
        detailReport << "\nFAIL: " << elementTypeName << " counts do not match.\n";
        addFailReason(
            failReasons,
            "element count mismatch for " + elementTypeName);
        aggregateStatus = ComparisonStatus::Fail;
    }

    const std::map<std::string, size_t> originalIndex = buildRecordIndex(originalRecords);
    const std::map<std::string, size_t> newIndex = buildRecordIndex(newRecords);

    for (const auto& [id, originalRecordIndex] : originalIndex)
    {
        if (!newIndex.contains(id))
        {
            detailReport << "FAIL: Original " << elementTypeName
                << " ID missing in newly created file: " << id << "\n";
            addFailReason(
                failReasons,
                "element in the original regression model found that is not in the newly created: " +
                    elementTypeName + " " + id);
            aggregateStatus = ComparisonStatus::Fail;
        }
    }

    for (const auto& [id, newRecordIndex] : newIndex)
    {
        if (!originalIndex.contains(id))
        {
            detailReport << "FAIL: Newly created " << elementTypeName
                << " ID not found in original regression file: " << id << "\n";
            aggregateStatus = ComparisonStatus::Fail;
        }
    }

    if (!periodsMatch)
        return ComparisonStatus::Fail;

    for (const auto& [id, originalRecordIndex] : originalIndex)
    {
        const auto newRecordIndex = newIndex.find(id);
        if (newRecordIndex == newIndex.end())
            continue;

        const TimeSeriesResultRecord& originalRecord = originalRecords[originalRecordIndex];
        const TimeSeriesResultRecord& newRecord = newRecords[newRecordIndex->second];

        for (size_t period = 0; period < originalRecord.times.size(); ++period)
        {
            if (period >= newRecord.times.size() ||
                originalRecord.times[period] != newRecord.times[period])
            {
                detailReport << "FAIL: Time stamp mismatch for " << elementTypeName << " " << id
                    << " at period " << period
                    << " original=" << (period < originalRecord.times.size() ? formatDateTime(originalRecord.times[period]) : "<missing>")
                    << " new=" << (period < newRecord.times.size() ? formatDateTime(newRecord.times[period]) : "<missing>")
                    << "\n";
                addFailReason(
                    failReasons,
                    "time-step timestamp not aligned for " + elementTypeName);
                aggregateStatus = ComparisonStatus::Fail;
            }
        }

        combineStatus(
            aggregateStatus,
            compareTimeSeriesVariables(
            sectionTitle,
            originalRecord,
            newRecord,
            specs,
            detailReport,
            failReasons,
            elementTypeName,
            toleranceSettings));
    }

    return aggregateStatus;
}

ComparisonStatus compareSubcatchmentsForModel(
    const std::string& modelName,
    const fs::path& originalRunoffBinary,
    const fs::path& newRunoffBinary,
    const fs::path& originalRoutingBinary,
    const fs::path& newRoutingBinary,
    const fs::path& detailReportPath,
    std::vector<std::string>& failReasons,
    const ToleranceSettings& toleranceSettings,
    const std::vector<std::string>& toleranceOverrideMessages)
{
    fs::create_directories(detailReportPath.parent_path());
    std::ofstream detailReport(detailReportPath);
    if (!detailReport)
        throw std::runtime_error("Could not create detailed comparison report: " + detailReportPath.string());

    ComparisonStatus modelStatus = ComparisonStatus::Pass;

    detailReport << "Model: " << modelName << "\n";
    detailReport << "Original runoff binary: " << originalRunoffBinary << "\n";
    detailReport << "New runoff binary: " << newRunoffBinary << "\n\n";
    detailReport << "Original routing binary: " << originalRoutingBinary << "\n";
    detailReport << "New routing binary: " << newRoutingBinary << "\n\n";
    writeToleranceOverview(detailReport, toleranceSettings, toleranceOverrideMessages);

    if (!fs::exists(originalRunoffBinary))
    {
        detailReport << "FAIL: Original runoff binary not found.\n";
        addFailReason(failReasons, "binaries of one of the models not found: original runoff");
        return ComparisonStatus::Fail;
    }

    if (!fs::exists(newRunoffBinary))
    {
        detailReport << "FAIL: Newly created runoff binary not found.\n";
        addFailReason(failReasons, "binaries of one of the models not found: newly created runoff");
        return ComparisonStatus::Fail;
    }

    if (!fs::exists(originalRoutingBinary))
    {
        detailReport << "FAIL: Original routing binary not found.\n";
        addFailReason(failReasons, "binaries of one of the models not found: original routing");
        return ComparisonStatus::Fail;
    }

    if (!fs::exists(newRoutingBinary))
    {
        detailReport << "FAIL: Newly created routing binary not found.\n";
        addFailReason(failReasons, "binaries of one of the models not found: newly created routing");
        return ComparisonStatus::Fail;
    }

    const RunoffBinaryResults originalResults = readRunoffBinaryResults(originalRunoffBinary);
    const RunoffBinaryResults newResults = readRunoffBinaryResults(newRunoffBinary);
    const RoutingBinaryResults originalRoutingResults = readRoutingBinaryResults(originalRoutingBinary);
    const RoutingBinaryResults newRoutingResults = readRoutingBinaryResults(newRoutingBinary);

    detailReport << "Original subcatchment count: " << originalResults.layout.subcatchCount << "\n";
    for (const std::string& id : originalResults.layout.subcatchIDs)
        detailReport << "  original ID: " << id << "\n";

    detailReport << "\nNew subcatchment count: " << newResults.layout.subcatchCount << "\n";
    for (const std::string& id : newResults.layout.subcatchIDs)
        detailReport << "  new ID: " << id << "\n";

    if (originalResults.layout.subcatchCount != newResults.layout.subcatchCount)
    {
        detailReport << "\nFAIL: Subcatchment counts do not match.\n";
        addFailReason(failReasons, "element count mismatch for subcatchment");
        modelStatus = ComparisonStatus::Fail;
    }

    const std::map<std::string, size_t> originalIndex =
        buildRecordIndex(originalResults.subcatchmentRecords);
    const std::map<std::string, size_t> newIndex =
        buildRecordIndex(newResults.subcatchmentRecords);

    for (const auto& [id, originalRecordIndex] : originalIndex)
    {
        if (!newIndex.contains(id))
        {
            detailReport << "FAIL: Original subcatchment ID missing in newly created file: "
                << id << "\n";
            addFailReason(
                failReasons,
                "element in the original regression model found that is not in the newly created: subcatchment " + id);
            modelStatus = ComparisonStatus::Fail;
        }
    }

    for (const auto& [id, newRecordIndex] : newIndex)
    {
        if (!originalIndex.contains(id))
        {
            detailReport << "FAIL: Newly created subcatchment ID not found in original regression file: "
                << id << "\n";
            modelStatus = ComparisonStatus::Fail;
        }
    }

    const size_t originalTimeStepsFound = originalResults.subcatchmentRecords.empty() ?
        0 : originalResults.subcatchmentRecords.front().values.size();
    const size_t newTimeStepsFound = newResults.subcatchmentRecords.empty() ?
        0 : newResults.subcatchmentRecords.front().values.size();

    detailReport << "\nOriginal Nperiods: " << originalResults.layout.periodCount << "\n";
    detailReport << "New Nperiods     : " << newResults.layout.periodCount << "\n";
    detailReport << "\nTime-steps found (original): " << originalTimeStepsFound << "\n";
    detailReport << "Time-steps found (new)     : " << newTimeStepsFound << "\n";

    const bool periodsMatch = originalResults.layout.periodCount == newResults.layout.periodCount;
    if (!periodsMatch)
    {
            detailReport << "FAIL: Nperiods do not match. Time-step value comparison skipped.\n";
        addFailReason(failReasons, "time-step (Nperiods) not aligned: runoff");
        modelStatus = ComparisonStatus::Fail;
    }

    for (const auto& [id, originalRecordIndex] : originalIndex)
    {
        const auto newRecordIndex = newIndex.find(id);
        if (newRecordIndex == newIndex.end())
            continue;

        const TimeSeriesResultRecord& originalRecord =
            originalResults.subcatchmentRecords[originalRecordIndex];
        const TimeSeriesResultRecord& newRecord =
            newResults.subcatchmentRecords[newRecordIndex->second];

        if (periodsMatch)
        {
            for (size_t period = 0; period < originalRecord.times.size(); ++period)
            {
                if (period >= newRecord.times.size() ||
                    originalRecord.times[period] != newRecord.times[period])
                {
                    detailReport << "FAIL: Time stamp mismatch for subcatchment " << id
                        << " at period " << period
                        << " original=" << (period < originalRecord.times.size() ? formatDateTime(originalRecord.times[period]) : "<missing>")
                        << " new=" << (period < newRecord.times.size() ? formatDateTime(newRecord.times[period]) : "<missing>")
                        << "\n";
                    addFailReason(failReasons, "time-step timestamp not aligned for subcatchment");
                    modelStatus = ComparisonStatus::Fail;
                }
            }

            combineStatus(
                modelStatus,
                compareSubcatchmentTimeSeries(
                    originalRecord,
                    newRecord,
                    detailReport,
                    failReasons,
                    toleranceSettings));
        }

        if (originalRecordIndex < originalResults.subcatchmentSummaries.size() &&
            newRecordIndex->second < newResults.subcatchmentSummaries.size())
        {
            combineStatus(
                modelStatus,
                compareSubcatchmentSummaries(
                id,
                originalResults.subcatchmentSummaries[originalRecordIndex],
                newResults.subcatchmentSummaries[newRecordIndex->second],
                detailReport,
                failReasons,
                toleranceSettings));
        }
        else
        {
            detailReport << "FAIL: Missing subcatchment summary data for " << id << "\n";
            addFailReason(failReasons, "FAIL summary subcatchment missing data");
            modelStatus = ComparisonStatus::Fail;
        }
    }

    const std::vector<TimeSeriesVariableComparisonSpec> hydrologyNodeSpecs =
    {
        { 0, "FLOW", ToleranceQuantity::Flow }
    };

    combineStatus(modelStatus, compareRecordCollections(
        "hydrology node",
        "Hydrology node",
        originalResults.hydrologyNodeRecords,
        newResults.hydrologyNodeRecords,
        hydrologyNodeSpecs,
        periodsMatch,
        detailReport,
        failReasons,
        toleranceSettings));

    const std::map<std::string, size_t> originalHydrologyNodeIndex =
        buildRecordIndex(originalResults.hydrologyNodeRecords);
    const std::map<std::string, size_t> newHydrologyNodeIndex =
        buildRecordIndex(newResults.hydrologyNodeRecords);

    for (const auto& [id, originalRecordIndex] : originalHydrologyNodeIndex)
    {
        const auto newRecordIndex = newHydrologyNodeIndex.find(id);
        if (newRecordIndex == newHydrologyNodeIndex.end())
            continue;

        if (originalRecordIndex < originalResults.hydrologyNodeSummaries.size() &&
            newRecordIndex->second < newResults.hydrologyNodeSummaries.size())
        {
            combineStatus(modelStatus, compareHydrologyNodeSummary(
                id,
                originalResults.hydrologyNodeSummaries[originalRecordIndex],
                newResults.hydrologyNodeSummaries[newRecordIndex->second],
                detailReport,
                failReasons,
                toleranceSettings));
        }
        else
        {
            detailReport << "FAIL: Missing hydrology node summary data for " << id << "\n";
            addFailReason(failReasons, "FAIL summary hydrology node missing data");
            modelStatus = ComparisonStatus::Fail;
        }
    }

    const std::vector<TimeSeriesVariableComparisonSpec> hydrologyLinkSpecs =
    {
        { 0, "INFLOW", ToleranceQuantity::Flow },
        { 1, "OUTFLOW", ToleranceQuantity::Flow }
    };

    combineStatus(modelStatus, compareRecordCollections(
        "hydrology link",
        "Hydrology link",
        originalResults.hydrologyLinkRecords,
        newResults.hydrologyLinkRecords,
        hydrologyLinkSpecs,
        periodsMatch,
        detailReport,
        failReasons,
        toleranceSettings));

    const std::map<std::string, size_t> originalHydrologyLinkIndex =
        buildRecordIndex(originalResults.hydrologyLinkRecords);
    const std::map<std::string, size_t> newHydrologyLinkIndex =
        buildRecordIndex(newResults.hydrologyLinkRecords);

    for (const auto& [id, originalRecordIndex] : originalHydrologyLinkIndex)
    {
        const auto newRecordIndex = newHydrologyLinkIndex.find(id);
        if (newRecordIndex == newHydrologyLinkIndex.end())
            continue;

        if (originalRecordIndex < originalResults.hydrologyLinkSummaries.size() &&
            newRecordIndex->second < newResults.hydrologyLinkSummaries.size())
        {
            combineStatus(modelStatus, compareHydrologyLinkSummary(
                id,
                originalResults.hydrologyLinkSummaries[originalRecordIndex],
                newResults.hydrologyLinkSummaries[newRecordIndex->second],
                detailReport,
                failReasons,
                toleranceSettings));
        }
        else
        {
            detailReport << "FAIL: Missing hydrology link summary data for " << id << "\n";
            addFailReason(failReasons, "FAIL summary hydrology link missing data");
            modelStatus = ComparisonStatus::Fail;
        }
    }

    const std::vector<TimeSeriesVariableComparisonSpec> hydrologyWSUDSpecs =
    {
        { 0, "HYD_WSUD_DEPTH", ToleranceQuantity::Depth },
        { 1, "HYD_WSUD_INFLOW", ToleranceQuantity::Flow },
        { 2, "HYD_WSUD_TREATEDFLOW", ToleranceQuantity::Flow },
        { 3, "HYD_WSUD_OVERFLOW", ToleranceQuantity::Flow },
        { 4, "HYD_WSUD_LOWBYPASSFLOW", ToleranceQuantity::Flow },
        { 5, "HYD_WSUD_HIGHBYPASSFLOW", ToleranceQuantity::Flow },
        { 6, "HYD_WSUD_REUSEFLOW", ToleranceQuantity::Flow },
        { 8, "HYD_WSUD_EVAPFLOW", ToleranceQuantity::Flow },
        { 9, "HYD_WSUD_INFILFLOW", ToleranceQuantity::Flow },
        { 10, "HYD_WSUD_STORAGE", ToleranceQuantity::Volume },
        { 11, "HYD_WSUD_SOIL_THETA", ToleranceQuantity::PercentPoint },
        { 7, "HYD_WSUD_REQUESTED_DEMAND", ToleranceQuantity::Flow }
    };

    combineStatus(modelStatus, compareRecordCollections(
        "hydrology WSUD",
        "Hydrology WSUD",
        originalResults.hydrologyWSUDRecords,
        newResults.hydrologyWSUDRecords,
        hydrologyWSUDSpecs,
        periodsMatch,
        detailReport,
        failReasons,
        toleranceSettings));

    const std::map<std::string, size_t> originalHydrologyWSUDIndex =
        buildRecordIndex(originalResults.hydrologyWSUDRecords);
    const std::map<std::string, size_t> newHydrologyWSUDIndex =
        buildRecordIndex(newResults.hydrologyWSUDRecords);

    for (const auto& [id, originalRecordIndex] : originalHydrologyWSUDIndex)
    {
        const auto newRecordIndex = newHydrologyWSUDIndex.find(id);
        if (newRecordIndex == newHydrologyWSUDIndex.end())
            continue;

        if (originalRecordIndex < originalResults.hydrologyWSUDSummaries.size() &&
            newRecordIndex->second < newResults.hydrologyWSUDSummaries.size())
        {
            combineStatus(modelStatus, compareHydrologyWSUDSummary(
                id,
                originalResults.hydrologyWSUDSummaries[originalRecordIndex],
                newResults.hydrologyWSUDSummaries[newRecordIndex->second],
                detailReport,
                failReasons,
                toleranceSettings));
        }
        else
        {
            detailReport << "FAIL: Missing hydrology WSUD summary data for " << id << "\n";
            addFailReason(failReasons, "FAIL summary hydrology WSUD missing data");
            modelStatus = ComparisonStatus::Fail;
        }
    }

    const bool routingPeriodsMatch =
        originalRoutingResults.layout.periodCount == newRoutingResults.layout.periodCount;
    detailReport << "\nOriginal routing Nperiods: " << originalRoutingResults.layout.periodCount << "\n";
    detailReport << "New routing Nperiods     : " << newRoutingResults.layout.periodCount << "\n";

    if (!routingPeriodsMatch)
    {
        detailReport << "FAIL: Routing Nperiods do not match. Hydraulic time-step value comparison skipped.\n";
        addFailReason(failReasons, "time-step (Nperiods) not aligned: routing");
        modelStatus = ComparisonStatus::Fail;
    }

    const std::vector<TimeSeriesVariableComparisonSpec> hydraulicNodeSpecs =
    {
        { 0, "DEPTH", ToleranceQuantity::Depth },
        { 2, "VOLUME", ToleranceQuantity::Volume },
        { 3, "LATERAL_FLOW", ToleranceQuantity::Flow },
        { 4, "INFLOW", ToleranceQuantity::Flow },
        { 5, "OVERFLOW", ToleranceQuantity::Flow }
    };

    combineStatus(modelStatus, compareRecordCollections(
        "hydraulic node",
        "Hydraulic node",
        originalRoutingResults.hydraulicNodeRecords,
        newRoutingResults.hydraulicNodeRecords,
        hydraulicNodeSpecs,
        routingPeriodsMatch,
        detailReport,
        failReasons,
        toleranceSettings));

    const std::map<std::string, size_t> originalHydraulicNodeIndex =
        buildRecordIndex(originalRoutingResults.hydraulicNodeRecords);
    const std::map<std::string, size_t> newHydraulicNodeIndex =
        buildRecordIndex(newRoutingResults.hydraulicNodeRecords);

    for (const auto& [id, originalRecordIndex] : originalHydraulicNodeIndex)
    {
        const auto newRecordIndex = newHydraulicNodeIndex.find(id);
        if (newRecordIndex == newHydraulicNodeIndex.end())
            continue;

        if (originalRecordIndex < originalRoutingResults.hydraulicNodeSummaries.size() &&
            newRecordIndex->second < newRoutingResults.hydraulicNodeSummaries.size())
        {
            combineStatus(modelStatus, compareHydraulicNodeSummary(
                id,
                originalRoutingResults.hydraulicNodeSummaries[originalRecordIndex],
                newRoutingResults.hydraulicNodeSummaries[newRecordIndex->second],
                detailReport,
                failReasons,
                toleranceSettings));
        }
        else
        {
            detailReport << "FAIL: Missing hydraulic node summary data for " << id << "\n";
            addFailReason(failReasons, "FAIL summary hydraulic node missing data");
            modelStatus = ComparisonStatus::Fail;
        }
    }

    const std::vector<TimeSeriesVariableComparisonSpec> hydraulicLinkSpecs =
    {
        { 0, "FLOW", ToleranceQuantity::Flow },
        { 1, "DEPTH", ToleranceQuantity::Depth },
        { 2, "VELOCITY", ToleranceQuantity::Velocity },
        { 3, "VOLUME", ToleranceQuantity::Volume }
    };

    combineStatus(modelStatus, compareRecordCollections(
        "hydraulic link",
        "Hydraulic link",
        originalRoutingResults.hydraulicLinkRecords,
        newRoutingResults.hydraulicLinkRecords,
        hydraulicLinkSpecs,
        routingPeriodsMatch,
        detailReport,
        failReasons,
        toleranceSettings));

    const std::map<std::string, size_t> originalHydraulicLinkIndex =
        buildRecordIndex(originalRoutingResults.hydraulicLinkRecords);
    const std::map<std::string, size_t> newHydraulicLinkIndex =
        buildRecordIndex(newRoutingResults.hydraulicLinkRecords);

    for (const auto& [id, originalRecordIndex] : originalHydraulicLinkIndex)
    {
        const auto newRecordIndex = newHydraulicLinkIndex.find(id);
        if (newRecordIndex == newHydraulicLinkIndex.end())
            continue;

        if (originalRecordIndex < originalRoutingResults.hydraulicLinkSummaries.size() &&
            newRecordIndex->second < newRoutingResults.hydraulicLinkSummaries.size())
        {
            combineStatus(modelStatus, compareHydraulicLinkSummary(
                id,
                originalRoutingResults.hydraulicLinkSummaries[originalRecordIndex],
                newRoutingResults.hydraulicLinkSummaries[newRecordIndex->second],
                detailReport,
                failReasons,
                toleranceSettings));
        }
        else
        {
            detailReport << "FAIL: Missing hydraulic link summary data for " << id << "\n";
            addFailReason(failReasons, "FAIL summary hydraulic link missing data");
            modelStatus = ComparisonStatus::Fail;
        }
    }

    detailReport << "\nModel result: " << comparisonStatusText(modelStatus) << "\n";
    return modelStatus;
}

int compareSubcatchmentsForAllModels(
    const fs::path& inpFolder,
    const fs::path& baseOutputFolder)
{
    const fs::path mainReportPath = baseOutputFolder / "comparison_summary.txt";
    std::ofstream mainReport(mainReportPath);
    if (!mainReport)
    {
        std::cerr << "WARNING: Could not create main comparison report: "
            << mainReportPath << "\n";
        return 1;
    }

    int failedModels = 0;
    int warnedModels = 0;
    int passedModels = 0;
    constexpr int mainReportModelWidth = 32;
    constexpr int mainReportResultWidth = 8;
    mainReport << std::left << std::setw(mainReportModelWidth) << "Model"
        << std::setw(mainReportResultWidth) << "Result"
        << "Fail reason\n";
    mainReport << "=================\n";

    for (const auto& entry : fs::directory_iterator(inpFolder))
    {
        if (!entry.is_directory())
            continue;

        const fs::path originalModelFolder = entry.path();
        const std::string modelName = originalModelFolder.filename().string();
        const fs::path newModelFolder = baseOutputFolder / modelName;
        const fs::path originalRunoffBinary =
            originalModelFolder / (modelName + "_runoff.bin");
        const fs::path newRunoffBinary =
            newModelFolder / (modelName + "_runoff.bin");
        const fs::path originalRoutingBinary =
            originalModelFolder / (modelName + ".bin");
        const fs::path newRoutingBinary =
            newModelFolder / (modelName + ".bin");
        const fs::path detailReportPath =
            newModelFolder / "comparison_result.txt";

        ComparisonStatus modelStatus = ComparisonStatus::Fail;
        std::vector<std::string> failReasons;
        std::vector<std::string> toleranceOverrideMessages;
        try
        {
            const ToleranceSettings toleranceSettings =
                readToleranceSettingsForModel(originalModelFolder, toleranceOverrideMessages);
            modelStatus = compareSubcatchmentsForModel(
                modelName,
                originalRunoffBinary,
                newRunoffBinary,
                originalRoutingBinary,
                newRoutingBinary,
                detailReportPath,
                failReasons,
                toleranceSettings,
                toleranceOverrideMessages);
        }
        catch (const std::exception& e)
        {
            addFailReason(failReasons, e.what());
            fs::create_directories(newModelFolder);
            std::ofstream detailReport(detailReportPath);
            if (detailReport)
            {
                detailReport << "Model: " << modelName << "\n";
                detailReport << "FAIL: " << e.what() << "\n";
            }
            std::cerr << "WARNING: Subcatchment comparison failed for model '"
                << modelName << "': " << e.what() << "\n";
            modelStatus = ComparisonStatus::Fail;
        }

        if (modelStatus == ComparisonStatus::Fail)
            ++failedModels;
        else if (modelStatus == ComparisonStatus::Warn)
            ++warnedModels;
        else
            ++passedModels;

        mainReport << std::left << std::setw(mainReportModelWidth) << modelName
            << std::setw(mainReportResultWidth) << comparisonStatusText(modelStatus);

        if (modelStatus != ComparisonStatus::Fail || failReasons.empty())
        {
            mainReport << "\n";
        }
        else
        {
            mainReport << failReasons.front() << "\n";
            for (size_t i = 1; i < failReasons.size(); ++i)
            {
                mainReport << std::setw(mainReportModelWidth) << ""
                    << std::setw(mainReportResultWidth) << ""
                    << failReasons[i] << "\n";
            }
        }
    }

    std::cout << "\nComparison summary\n";
    std::cout << "------------------\n";
    std::cout << "Passed: " << passedModels << "\n";
    std::cout << "Warned: " << warnedModels << "\n";
    std::cout << "Failed: " << failedModels << "\n";
    std::cout << "Main comparison report: " << mainReportPath << "\n";
    return failedModels;
}

int runProcess(const std::wstring& commandLine)
{
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    // CreateProcessW may modify the command-line buffer
    std::wstring cmd = commandLine;

    BOOL ok = CreateProcessW(
        nullptr,        // application name
        cmd.data(),     // command line
        nullptr,        // process security
        nullptr,        // thread security
        FALSE,          // inherit handles
        0,              // creation flags
        nullptr,        // environment
        nullptr,        // working directory
        &si,
        &pi
    );

    if (!ok)
    {
        DWORD err = GetLastError();
        std::wcerr << L"CreateProcessW failed. Error: " << err << L"\n";
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode))
    {
        std::wcerr << L"GetExitCodeProcess failed.\n";
        exitCode = static_cast<DWORD>(-1);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return static_cast<int>(exitCode);
}

using SwmmRunFunction = int(__stdcall*)(const char*, const char*, const char*);

std::string pathToUtf8String(const fs::path& path)
{
    return path.string();
}

int runEngineDll(
    const fs::path& enginePath,
    const fs::path& inpFile,
    const fs::path& outputFile,
    const fs::path& binaryFile)
{
    const fs::path dllPath = enginePath.parent_path() / "DrainsEngine.dll";
    HMODULE engineDll = LoadLibraryW(dllPath.wstring().c_str());
    if (!engineDll)
    {
        std::wcerr << L"LoadLibraryW failed for " << dllPath.wstring()
            << L". Error: " << GetLastError() << L"\n";
        return -1;
    }

    FARPROC proc = GetProcAddress(engineDll, "swmm_run");
    if (!proc)
    {
        std::wcerr << L"GetProcAddress failed for swmm_run in " << dllPath.wstring()
            << L". Error: " << GetLastError() << L"\n";
        FreeLibrary(engineDll);
        return -1;
    }

    SwmmRunFunction swmmRun = reinterpret_cast<SwmmRunFunction>(proc);
    const std::string inpFileArg = pathToUtf8String(inpFile);
    const std::string outputFileArg = pathToUtf8String(outputFile);
    const std::string binaryFileArg = pathToUtf8String(binaryFile);

    const int code = swmmRun(
        inpFileArg.c_str(),
        outputFileArg.c_str(),
        binaryFileArg.c_str());

    FreeLibrary(engineDll);
    return code;
}

int runEngineExe(
    const fs::path& enginePath,
    const fs::path& inpFile,
    const fs::path& outputFile,
    const fs::path& binaryFile)
{
    std::wstring cmd =
        L"\"" + enginePath.wstring() +
        L"\" -d \"" + inpFile.wstring() +
        L"\" \"" + outputFile.wstring() +
        L"\" \"" + binaryFile.wstring() +
        L"\"";

    return runProcess(cmd);
}

int runEngine_onAllInputFiles(char* argv[], bool useDll)
{
    fs::path enginePath = fs::absolute(argv[1]);
    fs::path inpFolder = fs::absolute(argv[2]);
    fs::path baseOutputFolder = fs::absolute(argv[3]);

    if (!fs::exists(enginePath))
    {
        std::cerr << "Engine not found: " << enginePath << "\n";
        return 2;
    }

    if (!fs::exists(inpFolder) || !fs::is_directory(inpFolder))
    {
        std::cerr << "Input folder not found or not a directory: " << inpFolder << "\n";
        return 3;
    }


    int failedRuns = 0;
    int passedRuns = 0;

    for (const auto& entry : fs::directory_iterator(inpFolder))
    {
        if (!entry.is_regular_file())
            continue;

        fs::path inpFile = entry.path();

        if (inpFile.extension() != ".inp" &&
            inpFile.extension() != ".INP")
            continue;

        fs::path outputFolder =
            baseOutputFolder /
            inpFile.stem();

        try
        {
            fs::create_directories(outputFolder);
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Could not create output folder: " << outputFolder << "\n";
            std::cerr << e.what() << "\n";
            ++failedRuns;
            printf("");
            continue;
        }

        fs::path outputFile = outputFolder / inpFile.filename();
        outputFile.replace_extension(".out");

        fs::path binaryFile = outputFolder / inpFile.filename();
        binaryFile.replace_extension(".bin");

        std::wcout << L"\nRunning: " << inpFile.wstring() << L"\n";
        std::wcout << L"Output folder: " << outputFolder.wstring() << L"\n";
        std::wcout << L"Output file  : " << outputFile.wstring() << L"\n";
        std::wcout << L"Binary file  : " << binaryFile.wstring() << L"\n";

        int code = useDll ?
            runEngineDll(enginePath, inpFile, outputFile, binaryFile) :
            runEngineExe(enginePath, inpFile, outputFile, binaryFile);

        if (code == 0)
        {
            ++passedRuns;
            std::wcout << L"PASS: " << inpFile.filename().wstring() << L"\n";
        }
        else
        {
            ++failedRuns;
            std::wcout << L"FAIL: " << inpFile.filename().wstring()
                << L" ExitCode=" << code << L"\n";
        }
    }

    std::cout << "\nSummary\n";
    std::cout << "-------\n";
    std::cout << "Engine runs\n";
    std::cout << "Passed: " << passedRuns << "\n";
    std::cout << "Failed: " << failedRuns << "\n";

    return failedRuns;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: (exe) TestAutomation.exe <Engine.exe> <InpFolder> <OutputFolder>\n         (dll) TestAutomation.exe <Engine.exe> <InpFolder> <OutputFolder> DLL";
        return 1;
    }

    const bool useDll =
        argc >= 5 &&
        (_stricmp(argv[4], "DLL") == 0);

    fs::path inpFolder = fs::absolute(argv[2]);
    fs::path baseOutputFolder = fs::absolute(argv[3]);

    int engineFails = runEngine_onAllInputFiles(argv, useDll);
    int comparisonFails = compareSubcatchmentsForAllModels(inpFolder, baseOutputFolder);
    int binaryReadFails = exportTemporaryBinaryTimeSeriesResults(baseOutputFolder);

    return (std::max)((std::max)(engineFails, comparisonFails), binaryReadFails);
}
