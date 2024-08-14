#pragma once

#include "holder_custom.h"
#include "shootingobject.h"
#include "physicsshellholder.h"
#include "hudsound.h"
class CCartridge;
class CCameraBase;

#ifndef CWEAPONSTATMGUN_CHANGE
#define CWEAPONSTATMGUN_CHANGE
#endif

#define DESIRED_DIR 1

#ifdef CWEAPONSTATMGUN_CHANGE
#define CWEAPONSTATMGUN_FIRE_DEBUG
#endif

class CWeaponStatMgun : public CPhysicsShellHolder,
                        public CHolderCustom,
                        public CShootingObject
{
private:
	typedef CPhysicsShellHolder inheritedPH;
	typedef CHolderCustom inheritedHolder;
	typedef CShootingObject inheritedShooting;

private:
#ifdef CWEAPONSTATMGUN_CHANGE
	enum EStmCamType{
		ectFirst = 0,
		ectChase,
	};
	CCameraBase *camera[2];
	CCameraBase *active_camera;
#else
	CCameraBase* camera;
#endif
	// 
	static void _BCL BoneCallbackX(CBoneInstance* B);
	static void _BCL BoneCallbackY(CBoneInstance* B);
	void SetBoneCallbacks();
	void ResetBoneCallbacks();

	HUD_SOUND_COLLECTION_LAYERED m_sounds;

	//casts
public:
	virtual CHolderCustom* cast_holder_custom() { return this; }

	//general
public:
	CWeaponStatMgun();
	virtual ~CWeaponStatMgun();

	virtual void Load(LPCSTR section);

	virtual BOOL net_Spawn(CSE_Abstract* DC);
	virtual void net_Destroy();
	virtual void net_Export(NET_Packet& P); // export to server
	virtual void net_Import(NET_Packet& P); // import from server

	virtual void UpdateCL();

	virtual void Hit(SHit* pHDS);

	//shooting
private:
	u16 m_rotate_x_bone, m_rotate_y_bone, m_fire_bone, m_camera_bone;
	float m_tgt_x_rot, m_tgt_y_rot, m_cur_x_rot, m_cur_y_rot, m_bind_x_rot, m_bind_y_rot;
	Fvector m_bind_x, m_bind_y;
	Fvector m_fire_dir, m_fire_pos;

	Fmatrix m_i_bind_x_xform, m_i_bind_y_xform, m_fire_bone_xform;
	Fvector2 m_lim_x_rot, m_lim_y_rot; //in bone space
	CCartridge* m_Ammo;
	float m_barrel_speed;
	Fvector2 m_dAngle;
	Fvector m_destEnemyDir;
	bool m_allow_fire;
	float camRelaxSpeed;
	float camMaxAngle;

	bool m_firing_disabled;
	bool m_overheat_enabled;
	float m_overheat_value;
	float m_overheat_time_quant;
	float m_overheat_decr_quant;
	float m_overheat_threshold;
	shared_str m_overheat_particles;
	CParticlesObject* p_overheat;
protected:
	void UpdateBarrelDir();
	virtual const Fvector& get_CurrentFirePoint();
	virtual const Fmatrix& get_ParticlesXFORM();

	virtual void FireStart();
	virtual void FireEnd();
	virtual void UpdateFire();
	virtual void OnShot();
	void AddShotEffector();
	void RemoveShotEffector();
	void SetDesiredDir(float h, float p);
	virtual bool IsHudModeNow() { return false; };

	//HolderCustom
public:
	virtual bool Use(const Fvector& pos, const Fvector& dir, const Fvector& foot_pos) { return !Owner(); };
	virtual void OnMouseMove(int x, int y);
	virtual void OnKeyboardPress(int dik);
	virtual void OnKeyboardRelease(int dik);
	virtual void OnKeyboardHold(int dik);
	virtual CInventory* GetInventory() { return NULL; };
	virtual void cam_Update(float dt, float fov = 90.0f);

	virtual void renderable_Render();

	virtual bool attach_Actor(CGameObject* actor);
	virtual void detach_Actor();
	virtual bool allowWeapon() const { return false; };
	virtual bool HUDView() const { return true; };
	virtual Fvector ExitPosition();

#ifdef CWEAPONSTATMGUN_CHANGE
	virtual CCameraBase *Camera() { return active_camera; }
#else
	virtual CCameraBase* Camera() { return camera; };
#endif

	virtual void Action(u16 id, u32 flags);
	virtual void SetParam(int id, Fvector2 val);

#ifdef CWEAPONSTATMGUN_CHANGE
public:
	enum {
		eWpnFire = 0,
		eWpnDesiredDef,
		eWpnDesiredPos,
		eWpnDesiredDir,
	};
	virtual void SetParam(int id, Fvector val);

private:
	float m_min_gun_speed;
	float m_max_gun_speed;
	bool m_turn_default;

	u16 m_actor_bone;
	u16 m_exit_bone;
	Fvector m_exit_position;
	u16 m_camera_bone_def;
	u16 m_camera_bone_aim;
	float m_zoom_factor_def;
	float m_zoom_factor_aim;
	bool m_zoom_status;
	Fvector m_camera_position;

	LPCSTR m_animation;

	static void IgnoreCollisionCallback(bool &do_colide, bool bo1, dContact &c, SGameMtl *mt1, SGameMtl *mt2);
	void OnCameraChange(u16 type);

public:
	virtual BOOL AlwaysTheCrow();
	virtual bool is_ai_obstacle() const;
	virtual CGameObject *cast_game_object() { return this; }
	virtual CPhysicsShellHolder *cast_physics_shell_holder() { return this; }

	bool IsCameraZoom() { return m_zoom_status; }
	LPCSTR Animation() { return m_animation; }
	CGameObject *GetOwner() { return Owner(); }

	float FireDirDiff();
	bool InFieldOfView(Fvector vec);
public:
DECLARE_SCRIPT_REGISTER_FUNCTION
#endif

};

#ifdef CWEAPONSTATMGUN_CHANGE
add_to_type_list(CWeaponStatMgun)
#undef script_type_list
#define script_type_list save_type_list(CWeaponStatMgun)
#endif