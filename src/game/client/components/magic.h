/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAGIC_H
#define GAME_CLIENT_COMPONENTS_MAGIC_H
#include <base/vmath.h>
#include <game/client/component.h>

class CMagic : public CComponent
{
	void DrawCircle(float x, float y, float r, int Segments);

	bool m_WasActive;
	bool m_Active;

	vec2 m_SelectorMouse;
	int m_SelectedMagic;

	static void ConKeyMagic(IConsole::IResult *pResult, void *pUserData);
	static void ConMagic(IConsole::IResult *pResult, void *pUserData);

public:
	CMagic();

	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnRender();
	virtual void OnRelease();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnCursorMove(float x, float y, int CursorType);

	void Magic(int Magic);
};

#endif
