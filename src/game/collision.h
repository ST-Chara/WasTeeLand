/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>
#include <base/tl/array.h>

class CCollision
{
	struct CTile *m_pTiles;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;
	bool *m_pBlocks;

	bool IsTile(int x, int y, int Flag = COLFLAG_SOLID) const;
	int GetTile(int x, int y) const;

public:
	enum
	{
		COLFLAG_SOLID = 1,
		COLFLAG_DEATH = 2,
		COLFLAG_NOHOOK = 4,
	};

	CCollision();
	void Init(class CLayers *pLayers);
	bool CheckPoint(float x, float y, int Flag = COLFLAG_SOLID) const { return IsTile(round_to_int(x), round_to_int(y), Flag); }
	bool CheckPoint(vec2 Pos, int Flag = COLFLAG_SOLID) const { return CheckPoint(Pos.x, Pos.y, Flag); }
	int GetCollisionAt(float x, float y) const { return GetTile(round_to_int(x), round_to_int(y)); }
	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const;
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool *pDeath = 0) const;
	bool TestBox(vec2 Pos, vec2 Size, int Flag = COLFLAG_SOLID) const;

	bool ModifTile(ivec2 pos, int group, int layer, int index, int flags, int reserved, bool limited = true, bool regen = true);
	void RegenerateSkip(CTile *pTiles, int Width, int Height, ivec2 Pos, bool Delete);

	bool GetBlock(int x, int y);
	void SetBlock(ivec2 Pos, bool Block);

	bool CheckBlocks(float x, float y) { return GetBlock(round_to_int(x), round_to_int(y)); }
	bool CheckBlocks(vec2 Pos) { return CheckBlocks(Pos.x, Pos.y); }

	int GetWidth() { return m_Width; }
	int GetHeight() { return m_Height; }
	int FastIntersectLine(vec2 Pos0, vec2 Pos1);
	bool IntersectBlocks(vec2 Pos0, vec2 Pos1);

	bool CanBuildBlock(int x, int y);
	bool CanBuildBlock(vec2 Pos) { return CanBuildBlock(round_to_int(Pos.x), round_to_int(Pos.y)); }
};

#endif
