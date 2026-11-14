#include "LimbInjuryPolicy.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{
    void Require(bool condition)
    {
        if (!condition)
            std::abort();
    }

    bool Near(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }
}

int main()
{
    using namespace LimbInjuryPolicy;

    LimbState state;
    state.trauma = 2;
    state.bleed = 1;
    state.splinted = true;
    state.heal = 0.5f;
    Require(Near(Encode(state), 121.5f));
    LimbState back = Decode(121.5f);
    Require(back.trauma == 2 && back.bleed == 1 && back.splinted && !back.tourniquet && Near(back.heal, 0.5f));

    for (int flags = 0; flags < 4; ++flags)
        for (int trauma = 0; trauma < 4; ++trauma)
            for (int bleed = 0; bleed < 3; ++bleed)
            {
                LimbState s;
                s.trauma = trauma;
                s.bleed = bleed;
                s.splinted = (flags & 1) != 0;
                s.tourniquet = (flags & 2) != 0;
                s.heal = 0.25f;
                const LimbState d = Decode(Encode(s));
                Require(d.trauma == trauma && d.bleed == bleed && d.splinted == s.splinted &&
                        d.tourniquet == s.tourniquet && Near(d.heal, 0.25f));
            }

    state = {};
    state.heal = 5.0f;
    Require(Near(Encode(state), 0.99f));
    state.heal = std::numeric_limits<float>::quiet_NaN();
    Require(Near(Encode(state), 0.0f));
    Require(Near(Decode(311.99f).heal, 0.99f) && Decode(311.99f).trauma == 1);

    for (const float bad : { -1.0f, 400.0f, 99999.0f, 4.0f, 40.0f, 5.0f, 8.0f,
                             std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
    {
        const LimbState d = Decode(bad);
        Require(d.trauma == 0 && d.bleed == 0 && !d.splinted && !d.tourniquet && Near(d.heal, 0.0f));
    }
    Require(Decode(30.0f).trauma == 3 && Decode(2.0f).bleed == 2 && Decode(300.0f).tourniquet && Decode(300.0f).splinted);

    Require(ClassifyHit(7.9f, HitCause::Ballistic, false, kLeftArm).trauma == 0);
    Require(ClassifyHit(8.0f, HitCause::Ballistic, false, kLeftArm).trauma == 1);
    Require(ClassifyHit(19.9f, HitCause::Melee, false, kHead).trauma == 1);
    Require(ClassifyHit(20.0f, HitCause::Melee, false, kHead).trauma == 2);
    Require(ClassifyHit(39.9f, HitCause::Ballistic, false, kTorso).trauma == 2);
    Require(ClassifyHit(40.0f, HitCause::Ballistic, false, kTorso).trauma == 3);
    Require(ClassifyHit(std::numeric_limits<float>::quiet_NaN(), HitCause::Ballistic, false, kTorso).trauma == 0);

    for (const auto cause : { HitCause::Ballistic, HitCause::Explosion })
    {
        Require(ClassifyHit(14.9f, cause, false, kTorso).bleed == 0);
        Require(ClassifyHit(15.0f, cause, false, kTorso).bleed == 1);
        Require(ClassifyHit(34.9f, cause, false, kTorso).bleed == 1);
        Require(ClassifyHit(35.0f, cause, false, kTorso).bleed == 2);
    }
    Require(ClassifyHit(24.9f, HitCause::Melee, false, kTorso).bleed == 0);
    Require(ClassifyHit(25.0f, HitCause::Melee, false, kTorso).bleed == 1);
    Require(ClassifyHit(44.9f, HitCause::Melee, false, kTorso).bleed == 1);
    Require(ClassifyHit(45.0f, HitCause::Melee, false, kTorso).bleed == 2);
    Require(ClassifyHit(100.0f, HitCause::Fall, false, kLeftLeg).bleed == 0);
    Require(ClassifyHit(100.0f, HitCause::Fall, false, kLeftLeg).trauma == 3);
    Require(ClassifyHit(100.0f, HitCause::Unknown, false, kLeftLeg).bleed == 0);

    Require(ClassifyHit(0.0f, HitCause::Melee, true, kLeftLeg).trauma == 2);
    Require(ClassifyHit(0.0f, HitCause::Ballistic, true, kRightArm).trauma == 2);
    Require(ClassifyHit(0.0f, HitCause::Explosion, true, kRightArm).trauma == 3);
    Require(ClassifyHit(0.0f, HitCause::Explosion, true, kLeftLeg).trauma == 3);
    Require(ClassifyHit(0.0f, HitCause::Fall, true, kRightLeg).trauma == 3);
    Require(ClassifyHit(0.0f, HitCause::Fall, true, kLeftArm).trauma == 3);
    Require(ClassifyHit(0.0f, HitCause::Explosion, true, kHead).trauma == 2);
    Require(ClassifyHit(0.0f, HitCause::Fall, true, kTorso).trauma == 2);
    Require(ClassifyHit(45.0f, HitCause::Melee, true, kLeftArm).trauma == 3);
    Require(ClassifyHit(25.0f, HitCause::Ballistic, true, kLeftArm).trauma == 2);
    Require(ClassifyHit(10.0f, HitCause::Ballistic, true, kLeftArm).trauma == 2);

    state = {};
    state.trauma = 1;
    state.heal = 0.5f;
    state = ApplyHit(state, { 1, 0 });
    Require(state.trauma == 2 && Near(state.heal, 0.0f));
    state = ApplyHit(state, { 1, 0 });
    Require(state.trauma == 2);
    state = ApplyHit(state, { 2, 0 });
    Require(state.trauma == 3);
    state = ApplyHit(state, { 3, 0 });
    Require(state.trauma == 3);
    state = ApplyHit(state, { 0, 0 });
    Require(state.trauma == 3);
    state = {};
    state.trauma = 2;
    state.splinted = true;
    state = ApplyHit(state, { 3, 0 });
    Require(state.trauma == 3 && !state.splinted);
    state = {};
    state.trauma = 2;
    state.splinted = true;
    state = ApplyHit(state, { 2, 0 });
    Require(state.trauma == 3 && !state.splinted);
    state = {};
    state.bleed = 2;
    state = ApplyHit(state, { 0, 1 });
    Require(state.bleed == 2);
    state.bleed = 1;
    state = ApplyHit(state, { 0, 2 });
    Require(state.bleed == 2);
    state = {};
    state.tourniquet = true;
    state = ApplyHit(state, { 0, 2 });
    Require(state.bleed == 0 && state.tourniquet);

    state = {};
    Require(EffectiveLevel(state) == 0);
    state.trauma = 3;
    Require(EffectiveLevel(state) == 3);
    state.splinted = true;
    Require(EffectiveLevel(state) == 2);
    state.tourniquet = true;
    Require(EffectiveLevel(state) == 3);
    state = {};
    state.trauma = 1;
    state.splinted = true;
    Require(EffectiveLevel(state) == 1);
    state = {};
    state.tourniquet = true;
    Require(EffectiveLevel(state) == 1);

    LimbStates states{};
    auto region = RegionLevels(states);
    Require(region.legs == 0 && region.arms == 0 && region.head == 0 && region.torso == 0);
    states[kLeftLeg].trauma = 2;
    region = RegionLevels(states);
    Require(region.legs == 2 && region.arms == 0);
    states[kRightLeg].trauma = 2;
    Require(RegionLevels(states).legs == 3);
    states[kRightLeg].trauma = 1;
    Require(RegionLevels(states).legs == 2);
    states[kLeftLeg].trauma = 3;
    states[kRightLeg].trauma = 3;
    Require(RegionLevels(states).legs == 4);
    states[kLeftLeg].splinted = true;
    states[kRightLeg].splinted = true;
    Require(RegionLevels(states).legs == 3);
    states[kLeftLeg].tourniquet = true;
    states[kRightLeg].tourniquet = true;
    Require(RegionLevels(states).legs == 4);
    states[kLeftArm].trauma = 1;
    states[kRightArm].trauma = 2;
    Require(RegionLevels(states).arms == 2);
    states[kLeftArm].trauma = 3;
    states[kRightArm].trauma = 3;
    Require(RegionLevels(states).arms == 4);
    states[kHead].trauma = 3;
    states[kHead].tourniquet = true;
    states[kTorso].trauma = 2;
    region = RegionLevels(states);
    Require(region.head == 3 && region.torso == 2);

    float clot = 0.0f;
    state = {};
    state.trauma = 1;
    Heal(state, 5.9f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1 && state.heal > 0.98f && state.heal < 0.99f);
    Heal(state, 0.2f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 0 && !state.splinted && Near(state.heal, 0.0f));

    state = {};
    state.trauma = 1;
    Heal(state, 3.0f, true, 1.0f, 0.0f, clot);
    Require(state.trauma == 0);
    state.trauma = 1;
    Heal(state, 3.0f, false, 2.0f, 0.0f, clot);
    Require(state.trauma == 0);
    state.trauma = 1;
    Heal(state, 3.0f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1 && Near(state.heal, 0.5f));
    Heal(state, 3.0f, false, 0.0f, 0.0f, clot);
    Require(state.trauma == 1 && Near(state.heal, 0.5f));
    Heal(state, -3.0f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1 && Near(state.heal, 0.5f));
    Heal(state, std::numeric_limits<float>::quiet_NaN(), false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1 && Near(state.heal, 0.5f));

    state = {};
    state.trauma = 2;
    Heal(state, 17.9f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 2);
    Heal(state, 0.2f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1);
    state = {};
    state.trauma = 2;
    state.splinted = true;
    Heal(state, 8.9f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 2);
    Heal(state, 0.2f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1 && state.splinted);
    Heal(state, 6.0f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 0 && !state.splinted);

    state = {};
    state.trauma = 3;
    Heal(state, 500.0f, true, 5.0f, 0.0f, clot);
    Require(state.trauma == 3 && Near(state.heal, 0.0f));
    state.splinted = true;
    Heal(state, 47.9f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 3 && state.splinted);
    Heal(state, 0.2f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 2 && state.splinted);
    state = {};
    state.trauma = 3;
    state.splinted = true;
    Heal(state, 24.0f, true, 1.0f, 0.0f, clot);
    Require(state.trauma == 2 && state.splinted && Near(state.heal, 0.0f));
    state = {};
    state.trauma = 3;
    state.splinted = true;
    Heal(state, 57.0f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 1 && state.splinted);
    Heal(state, 6.0f, false, 1.0f, 0.0f, clot);
    Require(state.trauma == 0 && !state.splinted);

    state = {};
    state.bleed = 1;
    clot = 0.0f;
    Heal(state, 0.0f, false, 1.0f, 60.0f, clot);
    Require(state.bleed == 1 && Near(clot, 60.0f));
    Heal(state, 0.0f, false, 1.0f, 29.0f, clot);
    Require(state.bleed == 1);
    Heal(state, 0.0f, false, 1.0f, 1.0f, clot);
    Require(state.bleed == 0 && Near(clot, 0.0f));
    state.bleed = 2;
    Heal(state, 100.0f, true, 1.0f, 100000.0f, clot);
    Require(state.bleed == 2 && Near(clot, 0.0f));
    state.bleed = 1;
    clot = 50.0f;
    Heal(state, 0.0f, false, 1.0f, 0.0f, clot);
    state.bleed = 2;
    Heal(state, 0.0f, false, 1.0f, 1.0f, clot);
    Require(Near(clot, 0.0f));

    states = {};
    states[kLeftArm].bleed = 2;
    states[kRightLeg].bleed = 1;
    Require(Treat(states, kTreatBandage));
    Require(states[kLeftArm].bleed == 1 && states[kRightLeg].bleed == 1);
    Require(Treat(states, kTreatBandage));
    Require(states[kLeftArm].bleed == 0 && states[kRightLeg].bleed == 1);
    Require(Treat(states, kTreatBandage));
    Require(IsClean(states));
    Require(!Treat(states, kTreatBandage));
    states[kHead].trauma = 2;
    Require(Treat(states, kTreatBandage));
    Require(states[kHead].splinted);
    Require(!Treat(states, kTreatBandage));
    states = {};
    states[kTorso].trauma = 1;
    states[kLeftLeg].trauma = 3;
    Require(!Treat(states, kTreatBandage));

    states = {};
    states[kRightLeg].trauma = 3;
    states[kLeftArm].trauma = 3;
    Require(Treat(states, kTreatSplint));
    Require(states[kRightLeg].splinted && !states[kLeftArm].splinted);
    Require(Treat(states, kTreatSplint));
    Require(states[kLeftArm].splinted);
    Require(!Treat(states, kTreatSplint));
    states = {};
    states[kLeftLeg].trauma = 2;
    states[kRightArm].trauma = 3;
    Require(Treat(states, kTreatSplint));
    Require(states[kRightArm].splinted && !states[kLeftLeg].splinted);
    Require(Treat(states, kTreatSplint));
    Require(states[kLeftLeg].splinted);
    Require(!Treat(states, kTreatSplint));
    states = {};
    states[kHead].trauma = 3;
    states[kTorso].trauma = 2;
    states[kLeftArm].trauma = 1;
    Require(!Treat(states, kTreatSplint));

    states = {};
    states[kLeftLeg].bleed = 2;
    states[kRightArm].bleed = 1;
    states[kHead].bleed = 2;
    Require(Treat(states, kTreatTourniquet));
    Require(states[kLeftLeg].bleed == 0 && states[kLeftLeg].tourniquet);
    Require(!Treat(states, kTreatTourniquet));
    Require(states[kHead].bleed == 2 && states[kRightArm].bleed == 1 && !states[kRightArm].tourniquet);
    states[kRightArm].bleed = 2;
    Require(Treat(states, kTreatTourniquet));
    Require(states[kRightArm].tourniquet && states[kRightArm].bleed == 0);

    states = {};
    Require(!Treat(states, kTreatSutureKit));
    states[kRightLeg].tourniquet = true;
    states[kRightLeg].trauma = 2;
    states[kRightLeg].splinted = true;
    states[kTorso].bleed = 2;
    Require(Treat(states, kTreatSutureKit));
    Require(states[kTorso].bleed == 0 && states[kRightLeg].tourniquet);
    Require(Treat(states, kTreatSutureKit));
    Require(!states[kRightLeg].tourniquet && states[kRightLeg].trauma == 2 && Near(states[kRightLeg].heal, 0.25f));
    Require(!Treat(states, kTreatSutureKit));
    states = {};
    states[kLeftArm].trauma = 1;
    states[kLeftArm].bleed = 1;
    states[kLeftArm].heal = 0.9f;
    Require(Treat(states, kTreatSutureKit));
    Require(states[kLeftArm].trauma == 0 && states[kLeftArm].bleed == 0 && Near(states[kLeftArm].heal, 0.0f));
    states = {};
    states[kLeftLeg].trauma = 3;
    states[kLeftLeg].bleed = 1;
    Require(Treat(states, kTreatSutureKit));
    Require(states[kLeftLeg].trauma == 3 && Near(states[kLeftLeg].heal, 0.0f) && states[kLeftLeg].bleed == 0);

    states = {};
    Require(!Treat(states, kTreatMedKit));
    states[kHead].trauma = 1;
    states[kLeftArm].trauma = 2;
    states[kLeftArm].bleed = 1;
    states[kRightLeg].trauma = 3;
    states[kRightLeg].tourniquet = true;
    states[kTorso].bleed = 2;
    Require(Treat(states, kTreatMedKit));
    Require(states[kHead].trauma == 0 && !states[kHead].splinted);
    Require(states[kLeftArm].trauma == 2 && states[kLeftArm].splinted && states[kLeftArm].bleed == 0 && Near(states[kLeftArm].heal, 0.25f));
    Require(states[kRightLeg].trauma == 3 && states[kRightLeg].splinted && !states[kRightLeg].tourniquet && Near(states[kRightLeg].heal, 0.25f));
    Require(states[kTorso].bleed == 0);
    Require(!Treat(states, kTreatSutureKit));
    Require(!Treat(states, kTreatNone));
    Require(!Treat(states, 99));
    Require(!Treat(states, -1));

    states = {};
    states[kHead].trauma = 1;
    states[kTorso].trauma = 2;
    states[kTorso].heal = 0.5f;
    states[kLeftArm].trauma = 2;
    states[kLeftArm].heal = 0.8f;
    states[kRightArm].trauma = 3;
    states[kLeftLeg].trauma = 3;
    states[kLeftLeg].splinted = true;
    states[kRightLeg].bleed = 2;
    states[kHead].bleed = 1;
    ApplyStimpak(states);
    Require(states[kHead].trauma == 0 && states[kHead].bleed == 0);
    Require(states[kTorso].trauma == 2 && Near(states[kTorso].heal, 0.75f));
    Require(states[kLeftArm].trauma == 1 && Near(states[kLeftArm].heal, 0.05f));
    Require(states[kRightArm].trauma == 3 && !states[kRightArm].splinted && Near(states[kRightArm].heal, 0.0f));
    Require(states[kLeftLeg].trauma == 3 && Near(states[kLeftLeg].heal, 0.25f));
    Require(states[kRightLeg].bleed == 1);
    Require(HasUnsplintedFracture(states));
    ApplyStimpak(states);
    Require(states[kRightLeg].bleed == 0);
    Require(HasUnsplintedFracture(states));
    Require(Treat(states, kTreatSplint));
    Require(!HasUnsplintedFracture(states));

    Require(IsClean(LimbStates{}));
    return 0;
}
