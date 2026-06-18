//-----------------------------------------------------------------------------
//   tolerances.h
//
//   Project: DRAINS ENGINE REGRESSION TESTING
//
//   Central tolerance defaults for binary regression comparisons. Test-case
//   specific tolerance configuration files can override these values later.
//
//-----------------------------------------------------------------------------

#pragma once

enum class ToleranceQuantity
{
    Flow,
    Volume,
    Depth,
    WaterSurfaceProfile,
    Velocity,
    ContinuityError,
    Time,
    Count,
    PercentPoint,
    FlowRegime
};

struct ToleranceRule
{
    double warnAbovePercent = 0.0;
    double failAbovePercent = 0.0;

    double warnAboveAbsolute = 0.0;
    double failAboveAbsolute = 0.0;

    double nearZeroThreshold = 0.0;

    bool useAbsoluteAlways = false;
    bool useAbsoluteBelowNearZero = false;
};

struct ToleranceSettings
{
    ToleranceRule flow;
    ToleranceRule volume;
    ToleranceRule depth;
    ToleranceRule waterSurfaceProfile;
    ToleranceRule velocity;
    ToleranceRule continuityError;
    ToleranceRule time;
    ToleranceRule count;
    ToleranceRule percentPoint;
    ToleranceRule flowRegime;
};

namespace DefaultTolerances
{
    // Flow is stored and compared as m3/s. The document's 5 L/s near-zero
    // threshold is therefore 0.005 m3/s, and 0.5 L/s is 0.0005 m3/s.
    inline constexpr ToleranceRule Flow =
    {
        1.0,     // warn above percent
        2.0,     // fail above percent
        0.0005,  // warn above absolute
        0.0010,  // fail above absolute
        0.0050,  // near-zero threshold
        false,
        true
    };

    // Volume is normally percentage based. The absolute values are conservative
    // defaults for near-zero volumes and can be overridden per test case.
    inline constexpr ToleranceRule Volume =
    {
        1.0,
        2.0,
        0.001,
        0.01,
        1.0,
        false,
        true
    };

    // General hydraulic grade / water level / depth tolerance, in meters.
    inline constexpr ToleranceRule Depth =
    {
        0.0,
        0.0,
        0.002,
        0.005,
        0.0,
        true,
        false
    };

    // Less strict profile samples, in meters.
    inline constexpr ToleranceRule WaterSurfaceProfile =
    {
        0.0,
        0.0,
        0.005,
        0.010,
        0.0,
        true,
        false
    };

    // Velocity is stored and compared as m/s.
    inline constexpr ToleranceRule Velocity =
    {
        2.0,
        5.0,
        0.02,
        0.05,
        0.02,
        false,
        true
    };

    // Continuity error values are percentages / percentage points.
    inline constexpr ToleranceRule ContinuityError =
    {
        0.0,
        0.0,
        0.5,
        1.0,
        0.0,
        true,
        false
    };

    // Time differences are stored and compared in seconds.
    inline constexpr ToleranceRule Time =
    {
        0.0,
        0.0,
        120.0,
        300.0,
        0.0,
        true,
        false
    };

    // Generic event-count checks. Specific pump/overflow checks can override
    // these to allow one warning event and two failure events.
    inline constexpr ToleranceRule Count =
    {
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        true,
        false
    };

    // Efficiency, reliability, or other percentage-point differences.
    inline constexpr ToleranceRule PercentPoint =
    {
        0.0,
        0.0,
        1.0,
        2.0,
        0.0,
        true,
        false
    };

    // Classification checks pass only on exact equality; any change is a fail.
    inline constexpr ToleranceRule FlowRegime =
    {
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        true,
        false
    };
}

inline constexpr ToleranceSettings defaultToleranceSettings()
{
    return
    {
        DefaultTolerances::Flow,
        DefaultTolerances::Volume,
        DefaultTolerances::Depth,
        DefaultTolerances::WaterSurfaceProfile,
        DefaultTolerances::Velocity,
        DefaultTolerances::ContinuityError,
        DefaultTolerances::Time,
        DefaultTolerances::Count,
        DefaultTolerances::PercentPoint,
        DefaultTolerances::FlowRegime
    };
}

inline constexpr ToleranceRule defaultToleranceRule(ToleranceQuantity quantity)
{
    switch (quantity)
    {
    case ToleranceQuantity::Flow:
        return DefaultTolerances::Flow;
    case ToleranceQuantity::Volume:
        return DefaultTolerances::Volume;
    case ToleranceQuantity::Depth:
        return DefaultTolerances::Depth;
    case ToleranceQuantity::WaterSurfaceProfile:
        return DefaultTolerances::WaterSurfaceProfile;
    case ToleranceQuantity::Velocity:
        return DefaultTolerances::Velocity;
    case ToleranceQuantity::ContinuityError:
        return DefaultTolerances::ContinuityError;
    case ToleranceQuantity::Time:
        return DefaultTolerances::Time;
    case ToleranceQuantity::Count:
        return DefaultTolerances::Count;
    case ToleranceQuantity::PercentPoint:
        return DefaultTolerances::PercentPoint;
    case ToleranceQuantity::FlowRegime:
        return DefaultTolerances::FlowRegime;
    default:
        return DefaultTolerances::Flow;
    }
}

inline constexpr ToleranceRule toleranceRule(
    const ToleranceSettings& settings,
    ToleranceQuantity quantity)
{
    switch (quantity)
    {
    case ToleranceQuantity::Flow:
        return settings.flow;
    case ToleranceQuantity::Volume:
        return settings.volume;
    case ToleranceQuantity::Depth:
        return settings.depth;
    case ToleranceQuantity::WaterSurfaceProfile:
        return settings.waterSurfaceProfile;
    case ToleranceQuantity::Velocity:
        return settings.velocity;
    case ToleranceQuantity::ContinuityError:
        return settings.continuityError;
    case ToleranceQuantity::Time:
        return settings.time;
    case ToleranceQuantity::Count:
        return settings.count;
    case ToleranceQuantity::PercentPoint:
        return settings.percentPoint;
    case ToleranceQuantity::FlowRegime:
        return settings.flowRegime;
    default:
        return settings.flow;
    }
}
