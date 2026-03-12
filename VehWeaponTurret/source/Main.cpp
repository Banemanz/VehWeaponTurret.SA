#include "plugin.h"
#include "common.h"
#include "CPad.h"
#include "CTimer.h"
#include "CWeapon.h"
#include "CProjectileInfo.h"
#include "CPlayerPed.h"
#include "CVehicle.h"
#include "CAutomobile.h"
#include "CCamera.h"
#include "CCam.h"
#include "eCamMode.h"
#include "eEntityStatus.h"
#include "RenderWare.h"

#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace plugin;

namespace VehicleWeaponINI {

    static const int   kMaxVehicleConfigs = 32;
    static const float kPiF = 3.14159265358979323846f;
    static const float kTwoPiF = kPiF * 2.0f;
    static const float kDegToRad = kPiF / 180.0f;

    enum eAimMode {
        AIMMODE_FORWARD = 0,
        AIMMODE_DOOM_TURRET = 1
    };

    enum eTriggerMode {
        TRIGGER_ATTACK = 0,
        TRIGGER_RIGHT_MOUSE = 1,
        TRIGGER_CUSTOM_KEY = 2,
        TRIGGER_ATTACK_OR_RIGHT_MOUSE = 3,
        TRIGGER_ATTACK_OR_CUSTOM_KEY = 4,
        TRIGGER_RIGHT_MOUSE_OR_CUSTOM_KEY = 5
    };

    struct VehicleWeaponConfig {
        bool  enabled;
        int   vehicleModel;
        int   weaponType;
        int   aimMode;
        int   triggerMode;
        int   triggerKeyVK;
        bool  fireproof;
        bool  useMiscA;
        float fallbackX;
        float fallbackY;
        float fallbackZ;
        float muzzleStartOffset;
        float range;
    };

    static VehicleWeaponConfig gConfigs[kMaxVehicleConfigs];
    static int gConfigCount = 0;
    static char gIniPath[MAX_PATH] = { 0 };

    static CWeapon gWeapon(WEAPONTYPE_FTHROWER, 50000);
    static int gCurrentWeaponType = WEAPONTYPE_FTHROWER;

    extern "C" IMAGE_DOS_HEADER __ImageBase;

    static float ClampFloat(float value, float minValue, float maxValue) {
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }

    static float WrapAngle(float angle) {
        while (angle > kPiF)
            angle -= kTwoPiF;
        while (angle < -kPiF)
            angle += kTwoPiF;
        return angle;
    }

    static bool FileExists(const char* path) {
        DWORD attrib = GetFileAttributesA(path);
        return attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
    }

    static void BuildIniPath() {
        GetModuleFileNameA((HMODULE)&__ImageBase, gIniPath, MAX_PATH);

        char* dot = std::strrchr(gIniPath, '.');
        if (dot)
            std::strcpy(dot, ".ini");
        else
            std::strcat(gIniPath, ".ini");
    }

    static int ReadIniInt(const char* section, const char* key, int defaultValue) {
        return GetPrivateProfileIntA(section, key, defaultValue, gIniPath);
    }

    static float ReadIniFloat(const char* section, const char* key, float defaultValue) {
        char defaultText[64];
        char valueText[64];

        std::sprintf(defaultText, "%.6f", defaultValue);
        GetPrivateProfileStringA(section, key, defaultText, valueText, sizeof(valueText), gIniPath);

        return (float)std::atof(valueText);
    }

    static bool ReadIniBool(const char* section, const char* key, bool defaultValue) {
        return ReadIniInt(section, key, defaultValue ? 1 : 0) != 0;
    }

    static void SetDefaultConfig(VehicleWeaponConfig& cfg) {
        cfg.enabled = false;
        cfg.vehicleModel = -1;
        cfg.weaponType = WEAPONTYPE_FTHROWER;
        cfg.aimMode = AIMMODE_FORWARD;
        cfg.triggerMode = TRIGGER_ATTACK;
        cfg.triggerKeyVK = 0;
        cfg.fireproof = false;
        cfg.useMiscA = true;
        cfg.fallbackX = 0.0f;
        cfg.fallbackY = 0.0f;
        cfg.fallbackZ = 1.1f;
        cfg.muzzleStartOffset = 0.75f;
        cfg.range = 18.0f;
    }

    static void WriteDefaultIni() {
        WritePrivateProfileStringA("General", "ReloadKeyVK", "116", gIniPath);

        WritePrivateProfileStringA("Vehicle0", "Enabled", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "VehicleModel", "601", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "WeaponType", "37", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "AimMode", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "TriggerMode", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "TriggerKeyVK", "0", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "Fireproof", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "UseMiscA", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "FallbackX", "0.0", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "FallbackY", "0.0", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "FallbackZ", "1.1", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "MuzzleStartOffset", "0.75", gIniPath);
        WritePrivateProfileStringA("Vehicle0", "Range", "18.0", gIniPath);

        WritePrivateProfileStringA("Vehicle1", "Enabled", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "VehicleModel", "407", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "WeaponType", "42", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "AimMode", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "TriggerMode", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "TriggerKeyVK", "0", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "Fireproof", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "UseMiscA", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "FallbackX", "0.0", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "FallbackY", "0.0", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "FallbackZ", "1.1", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "MuzzleStartOffset", "0.75", gIniPath);
        WritePrivateProfileStringA("Vehicle1", "Range", "18.0", gIniPath);

        WritePrivateProfileStringA("Vehicle2", "Enabled", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "VehicleModel", "432", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "WeaponType", "35", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "AimMode", "0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "TriggerMode", "1", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "TriggerKeyVK", "0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "Fireproof", "0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "UseMiscA", "0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "FallbackX", "0.0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "FallbackY", "2.5", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "FallbackZ", "1.0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "MuzzleStartOffset", "1.0", gIniPath);
        WritePrivateProfileStringA("Vehicle2", "Range", "80.0", gIniPath);
    }

    static void LoadConfig() {
        gConfigCount = 0;

        if (!FileExists(gIniPath))
            WriteDefaultIni();

        for (int i = 0; i < kMaxVehicleConfigs; i++) {
            char section[32];
            std::sprintf(section, "Vehicle%d", i);

            VehicleWeaponConfig cfg;
            SetDefaultConfig(cfg);

            cfg.enabled = ReadIniBool(section, "Enabled", false);
            cfg.vehicleModel = ReadIniInt(section, "VehicleModel", -1);
            cfg.weaponType = ReadIniInt(section, "WeaponType", WEAPONTYPE_FTHROWER);
            cfg.aimMode = ReadIniInt(section, "AimMode", AIMMODE_FORWARD);
            cfg.triggerMode = ReadIniInt(section, "TriggerMode", TRIGGER_ATTACK);
            cfg.triggerKeyVK = ReadIniInt(section, "TriggerKeyVK", 0);
            cfg.fireproof = ReadIniBool(section, "Fireproof", false);
            cfg.useMiscA = ReadIniBool(section, "UseMiscA", true);
            cfg.fallbackX = ReadIniFloat(section, "FallbackX", 0.0f);
            cfg.fallbackY = ReadIniFloat(section, "FallbackY", 0.0f);
            cfg.fallbackZ = ReadIniFloat(section, "FallbackZ", 1.1f);
            cfg.muzzleStartOffset = ReadIniFloat(section, "MuzzleStartOffset", 0.75f);
            cfg.range = ReadIniFloat(section, "Range", 18.0f);

            if (!cfg.enabled)
                continue;

            if (cfg.vehicleModel < 0)
                continue;

            if (gConfigCount < kMaxVehicleConfigs)
                gConfigs[gConfigCount++] = cfg;
        }
    }

    static const VehicleWeaponConfig* FindVehicleConfig(int modelId) {
        for (int i = 0; i < gConfigCount; i++) {
            if (gConfigs[i].vehicleModel == modelId)
                return &gConfigs[i];
        }
        return 0;
    }

    static void EnsureWeaponType(int weaponType) {
        if (gCurrentWeaponType == weaponType)
            return;

        gWeapon.StopWeaponEffect();
        gWeapon = CWeapon((eWeaponType)weaponType, 50000);
        gCurrentWeaponType = weaponType;
    }

    static short GetCarGunLeftRight(CPad* pad) {
        return plugin::CallMethodAndReturn<short, 0x53FC50, CPad*>(pad);
    }

    static short GetCarGunUpDown(CPad* pad) {
        return plugin::CallMethodAndReturn<short, 0x53FC10, CPad*>(pad);
    }

    static bool IsAttackPressed(CPad* pad) {
        if (!pad)
            return false;

        return pad->GetCarGunFired() != 0 || pad->NewState.ButtonCircle != 0;
    }

    static bool IsRightMousePressed() {
        return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    }

    static bool IsVirtualKeyPressed(int vk) {
        if (vk <= 0)
            return false;

        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    static bool IsTriggerPressed(const VehicleWeaponConfig* cfg, CPad* pad) {
        if (!cfg)
            return false;

        const bool attackPressed = IsAttackPressed(pad);
        const bool rightMousePressed = IsRightMousePressed();
        const bool customKeyPressed = IsVirtualKeyPressed(cfg->triggerKeyVK);

        switch (cfg->triggerMode) {
        case TRIGGER_ATTACK:
            return attackPressed;
        case TRIGGER_RIGHT_MOUSE:
            return rightMousePressed;
        case TRIGGER_CUSTOM_KEY:
            return customKeyPressed;
        case TRIGGER_ATTACK_OR_RIGHT_MOUSE:
            return attackPressed || rightMousePressed;
        case TRIGGER_ATTACK_OR_CUSTOM_KEY:
            return attackPressed || customKeyPressed;
        case TRIGGER_RIGHT_MOUSE_OR_CUSTOM_KEY:
            return rightMousePressed || customKeyPressed;
        default:
            return attackPressed;
        }
    }

    static bool IsVehicleDead(CVehicle* vehicle) {
        if (!vehicle)
            return true;

        if (vehicle->bDestroyed)
            return true;

        if (vehicle->m_nStatus == STATUS_WRECKED)
            return true;

        if (vehicle->m_fHealth <= 0.0f)
            return true;

        return false;
    }

    static CVector TransformLocalToWorld(CVehicle* vehicle, const CVector& local) {
        return vehicle->GetPosition()
            + vehicle->GetRight() * local.x
            + vehicle->GetForward() * local.y
            + vehicle->GetUp() * local.z;
    }

    static CVector TransformLocalDirToWorld(CVehicle* vehicle, const CVector& localDir) {
        return vehicle->GetRight() * localDir.x
            + vehicle->GetForward() * localDir.y
            + vehicle->GetUp() * localDir.z;
    }

    static CVector TransformWorldDirToLocal(CVehicle* vehicle, const CVector& worldDir) {
        return CVector(
            DotProduct(worldDir, vehicle->GetRight()),
            DotProduct(worldDir, vehicle->GetForward()),
            DotProduct(worldDir, vehicle->GetUp())
        );
    }

    static void UpdateDoomTurretAim(CAutomobile* automobile, CPad* pad) {
        if (!automobile || !pad)
            return;

        CCam& activeCam = TheCamera.m_aCams[TheCamera.m_nActiveCam];

        if (activeCam.m_nMode != MODE_CAM_ON_A_STRING) {
            automobile->m_fDoomVerticalRotation -=
                ((float)GetCarGunLeftRight(pad) * (CTimer::ms_fTimeStep / 20.0f)) / 128.0f;

            automobile->m_fDoomHorizontalRotation +=
                ((float)GetCarGunUpDown(pad) * (CTimer::ms_fTimeStep / 50.0f)) / 128.0f;
        }
        else {
            CVector frontLocal = TransformWorldDirToLocal(automobile, activeCam.m_vecFront);

            float targetYaw = std::atan2(-frontLocal.x, frontLocal.y);
            float targetPitch = std::atan2(
                frontLocal.z,
                std::sqrt(frontLocal.x * frontLocal.x + frontLocal.y * frontLocal.y)
            );

            targetPitch += 22.0f * kDegToRad;

            if (targetYaw > automobile->m_fDoomVerticalRotation + kPiF)
                targetYaw -= kTwoPiF;
            else if (targetYaw < automobile->m_fDoomVerticalRotation - kPiF)
                targetYaw += kTwoPiF;

            {
                float yawDiff = targetYaw - automobile->m_fDoomVerticalRotation;
                float yawStep = CTimer::ms_fTimeStep / 20.0f;

                if (yawDiff > yawStep)
                    automobile->m_fDoomVerticalRotation += yawStep;
                else if (yawDiff < -yawStep)
                    automobile->m_fDoomVerticalRotation -= yawStep;
                else
                    automobile->m_fDoomVerticalRotation = targetYaw;
            }

            {
                float pitchDiff = targetPitch - automobile->m_fDoomHorizontalRotation;
                float pitchStep = CTimer::ms_fTimeStep / 50.0f;

                if (pitchDiff > pitchStep)
                    automobile->m_fDoomHorizontalRotation += pitchStep;
                else if (pitchDiff < -pitchStep)
                    automobile->m_fDoomHorizontalRotation -= pitchStep;
                else
                    automobile->m_fDoomHorizontalRotation = targetPitch;
            }
        }

        automobile->m_fDoomVerticalRotation = WrapAngle(automobile->m_fDoomVerticalRotation);

        automobile->m_fDoomHorizontalRotation = ClampFloat(
            automobile->m_fDoomHorizontalRotation,
            -10.0f * kDegToRad,
            35.0f * kDegToRad
        );
    }

    static bool GetForwardAimMuzzleAndTarget(CVehicle* vehicle, const VehicleWeaponConfig* cfg, CVector& outOrigin, CVector& outTarget) {
        if (!vehicle || !cfg)
            return false;

        CVector worldDir = vehicle->GetForward();
        worldDir.Normalise();

        CVector base = TransformLocalToWorld(vehicle, CVector(cfg->fallbackX, cfg->fallbackY, cfg->fallbackZ));

        outOrigin = base + worldDir * cfg->muzzleStartOffset;
        outTarget = outOrigin + worldDir * cfg->range;

        return true;
    }

    static bool GetDoomTurretMuzzleAndTarget(CAutomobile* automobile, const VehicleWeaponConfig* cfg, CVector& outOrigin, CVector& outTarget) {
        if (!automobile || !cfg)
            return false;

        CVector turretBase;
        bool hasMiscA = cfg->useMiscA && automobile->m_aCarNodes[CAR_MISC_A] != 0;

        if (hasMiscA) {
            RwMatrix* miscMatrix = RwFrameGetLTM(automobile->m_aCarNodes[CAR_MISC_A]);
            if (!miscMatrix)
                return false;

            turretBase = *reinterpret_cast<CVector*>(RwMatrixGetPos(miscMatrix));
        }
        else {
            turretBase = TransformLocalToWorld(automobile, CVector(cfg->fallbackX, cfg->fallbackY, cfg->fallbackZ));
        }

        float yaw = automobile->m_fDoomVerticalRotation;
        float pitch = automobile->m_fDoomHorizontalRotation;

        CVector localDir(
            -(std::sin(yaw) * std::cos(pitch)),
            (std::cos(yaw) * std::cos(pitch)),
            std::sin(pitch)
        );

        CVector worldDir = TransformLocalDirToWorld(automobile, localDir);
        worldDir.Normalise();

        outOrigin = turretBase + worldDir * cfg->muzzleStartOffset;
        outTarget = outOrigin + worldDir * cfg->range;

        return true;
    }

    static bool GetMuzzleAndTarget(CVehicle* vehicle, const VehicleWeaponConfig* cfg, CVector& outOrigin, CVector& outTarget) {
        if (!vehicle || !cfg)
            return false;

        if (cfg->aimMode == AIMMODE_DOOM_TURRET) {
            if (vehicle->m_nVehicleSubClass != VEHICLE_AUTOMOBILE)
                return false;

            return GetDoomTurretMuzzleAndTarget(static_cast<CAutomobile*>(vehicle), cfg, outOrigin, outTarget);
        }

        return GetForwardAimMuzzleAndTarget(vehicle, cfg, outOrigin, outTarget);
    }

    static bool IsProjectileWeaponType(eWeaponType weaponType) {
        switch (weaponType) {
        case WEAPONTYPE_GRENADE:
        case WEAPONTYPE_TEARGAS:
        case WEAPONTYPE_MOLOTOV:
        case WEAPONTYPE_RLAUNCHER:
        case WEAPONTYPE_RLAUNCHER_HS:
        case WEAPONTYPE_FREEFALL_BOMB:
            return true;
        default:
            return false;
        }
    }

    static eWeaponType GetProjectileSpawnType(eWeaponType weaponType) {
        switch (weaponType) {
        case WEAPONTYPE_RLAUNCHER:
            return WEAPONTYPE_ROCKET;
        case WEAPONTYPE_RLAUNCHER_HS:
            return WEAPONTYPE_ROCKET_HS;
        case WEAPONTYPE_GRENADE:
            return WEAPONTYPE_GRENADE;
        case WEAPONTYPE_TEARGAS:
            return WEAPONTYPE_TEARGAS;
        case WEAPONTYPE_MOLOTOV:
            return WEAPONTYPE_MOLOTOV;
        case WEAPONTYPE_FREEFALL_BOMB:
            return WEAPONTYPE_FREEFALL_BOMB;
        default:
            return weaponType;
        }
    }

    static float GetProjectileForce(eWeaponType weaponType) {
        switch (weaponType) {
        case WEAPONTYPE_RLAUNCHER:
        case WEAPONTYPE_RLAUNCHER_HS:
            return 0.30f;
        case WEAPONTYPE_GRENADE:
            return 0.22f;
        case WEAPONTYPE_MOLOTOV:
            return 0.20f;
        case WEAPONTYPE_TEARGAS:
            return 0.18f;
        case WEAPONTYPE_FREEFALL_BOMB:
            return 0.10f;
        default:
            return 0.20f;
        }
    }

    static bool FireMountedProjectile(CEntity* firingEntity, eWeaponType weaponType, const CVector& origin, const CVector& target) {
        if (!firingEntity)
            return false;

        CVector dir = target - origin;
        float dirLen = dir.Magnitude();
        if (dirLen <= 0.001f)
            return false;

        dir /= dirLen;

        eWeaponType projectileType = GetProjectileSpawnType(weaponType);
        float force = GetProjectileForce(weaponType);

        return CProjectileInfo::AddProjectile(
            firingEntity,
            projectileType,
            origin,
            force,
            &dir,
            0
        );
    }

    static bool FireMountedWeapon(CPlayerPed* player, CVehicle* vehicle, const VehicleWeaponConfig* cfg, const CVector& origin, const CVector& target) {
        if (!player || !vehicle || !cfg)
            return false;

        eWeaponType weaponType = (eWeaponType)cfg->weaponType;

        if (IsProjectileWeaponType(weaponType))
            return FireMountedProjectile(vehicle, weaponType, origin, target);

        return gWeapon.Fire(
            player,
            const_cast<CVector*>(&origin),
            const_cast<CVector*>(&origin),
            0,
            const_cast<CVector*>(&target),
            0
        );
    }

    static void StopWeaponEffect() {
        gWeapon.StopWeaponEffect();
    }

    class Plugin {
    public:
        Plugin() {
            BuildIniPath();
            LoadConfig();
            Events::gameProcessEvent += Process;
        }

        static void Process() {
            int reloadKey = ReadIniInt("General", "ReloadKeyVK", 0);
            if (reloadKey > 0 && (GetAsyncKeyState(reloadKey) & 1))
                LoadConfig();

            CPlayerPed* player = FindPlayerPed();
            if (!player) {
                StopWeaponEffect();
                return;
            }

            CVehicle* vehicle = FindPlayerVehicle(-1, false);
            if (!vehicle || vehicle->m_pDriver != player || IsVehicleDead(vehicle)) {
                StopWeaponEffect();
                return;
            }

            const VehicleWeaponConfig* cfg = FindVehicleConfig(vehicle->m_nModelIndex);
            if (!cfg) {
                StopWeaponEffect();
                return;
            }

            EnsureWeaponType(cfg->weaponType);
            gWeapon.Update(player);

            if (cfg->fireproof) {
                vehicle->bFireProof = true;
                if (vehicle->m_pFire)
                    vehicle->ExtinguishCarFire();
            }

            CPad* pad = CPad::GetPad(0);
            if (!pad) {
                StopWeaponEffect();
                return;
            }

            if (cfg->aimMode == AIMMODE_DOOM_TURRET) {
                if (vehicle->m_nVehicleSubClass == VEHICLE_AUTOMOBILE)
                    UpdateDoomTurretAim(static_cast<CAutomobile*>(vehicle), pad);
            }

            if (!IsTriggerPressed(cfg, pad)) {
                StopWeaponEffect();
                return;
            }

            CVector origin;
            CVector target;

            if (!GetMuzzleAndTarget(vehicle, cfg, origin, target)) {
                StopWeaponEffect();
                return;
            }

            FireMountedWeapon(player, vehicle, cfg, origin, target);
        }
    };

    Plugin gPlugin;

} // namespace VehicleWeaponINI