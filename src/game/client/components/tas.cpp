#include "tas.h"
#include <base/system.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/components/controls.h>
#include <game/client/ui.h>
#include <fstream>
#include <sstream>

// CTASCharacter реализация
CTASCharacter::CTASCharacter(CGameWorld *pGameWorld, int ID, CNetObj_Character *pChar)
	: CCharacter(pGameWorld, ID, pChar)
{
	mem_zero(&m_Input, sizeof(m_Input));
}

void CTASCharacter::ApplyInput(CNetObj_PlayerInput *pInput)
{
	m_Input = *pInput;
	// Применяем ввод к персонажу
	OnDirectInput(pInput);
}

// CTASWorld реализация
CTASWorld::CTASWorld()
{
	m_pGameWorld = nullptr;
	m_pTASCharacter = nullptr;
	m_Recording = false;
	m_Replaying = false;
	m_Paused = false;
	m_CurrentTick = 0;
	m_ReplayIndex = 0;
	m_TPS = 50; // По умолчанию 50 TPS как в DDNet
}

CTASWorld::~CTASWorld()
{
	Reset();
}

void CTASWorld::Init(CGameWorld *pGameWorld, CCharacter *pCharacter)
{
	Reset();
	
	// Создаем копию игрового мира
	m_pGameWorld = new CGameWorld();
	m_pGameWorld->m_pCollision = pGameWorld->m_pCollision;
	m_pGameWorld->m_pTuningList = pGameWorld->m_pTuningList;
	
	// Создаем TAS персонажа на основе реального
	if(pCharacter)
	{
		CNetObj_Character CharObj;
		pCharacter->FillInfo(&CharObj);
		m_pTASCharacter = new CTASCharacter(m_pGameWorld, pCharacter->GetCID(), &CharObj);
		m_pTASCharacter->m_Pos = pCharacter->m_Pos;
		m_pTASCharacter->m_Core = pCharacter->m_Core;
	}
}

void CTASWorld::Reset()
{
	if(m_pTASCharacter)
	{
		delete m_pTASCharacter;
		m_pTASCharacter = nullptr;
	}
	
	if(m_pGameWorld)
	{
		delete m_pGameWorld;
		m_pGameWorld = nullptr;
	}
	
	m_vRecording.clear();
	m_Recording = false;
	m_Replaying = false;
	m_Paused = false;
	m_CurrentTick = 0;
	m_ReplayIndex = 0;
}

void CTASWorld::StartRecording()
{
	if(!m_pTASCharacter)
		return;
		
	m_Recording = true;
	m_Replaying = false;
	m_Paused = false;
	m_vRecording.clear();
	m_CurrentTick = 0;
}

void CTASWorld::StopRecording()
{
	m_Recording = false;
}

void CTASWorld::StartReplay()
{
	if(m_vRecording.empty())
		return;
		
	m_Replaying = true;
	m_Recording = false;
	m_Paused = false;
	m_CurrentTick = 0;
	m_ReplayIndex = 0;
}

void CTASWorld::StopReplay()
{
	m_Replaying = false;
}

void CTASWorld::Pause()
{
	m_Paused = true;
}

void CTASWorld::Resume()
{
	m_Paused = false;
}

void CTASWorld::Tick()
{
	if(!m_pGameWorld || !m_pTASCharacter || m_Paused)
		return;
		
	if(m_Replaying)
	{
		ReplayFrame();
	}
	
	// Обновляем TAS мир
	m_pGameWorld->Tick();
	m_CurrentTick++;
}

void CTASWorld::RecordFrame(CNetObj_PlayerInput *pInput)
{
	if(!m_Recording || !m_pTASCharacter)
		return;
		
	CTASFrame Frame;
	Frame.m_Tick = m_CurrentTick;
	Frame.m_Input = *pInput;
	Frame.m_Position = m_pTASCharacter->m_Pos;
	Frame.m_Velocity = m_pTASCharacter->GetVel();
	Frame.m_Flags = 0; // Можно добавить флаги состояния
	
	m_vRecording.push_back(Frame);
	
	// Применяем ввод к TAS персонажу
	m_pTASCharacter->ApplyInput(pInput);
}

bool CTASWorld::ReplayFrame()
{
	if(!m_Replaying || m_ReplayIndex >= (int)m_vRecording.size())
		return false;
		
	const CTASFrame &Frame = m_vRecording[m_ReplayIndex];
	
	if(Frame.m_Tick == m_CurrentTick)
	{
		// Применяем записанный ввод
		CNetObj_PlayerInput input = Frame.m_Input; // Создаем неконстантную копию
		m_pTASCharacter->ApplyInput(&input);
		m_ReplayIndex++;
		return true;
	}
	
	return false;
}

void CTASWorld::SaveToFile(const char *pFilename)
{
	std::ofstream File(pFilename);
	if(!File.is_open())
		return;
		
	File << "# DDNet TAS Recording\n";
	File << "# Format: tick direction targetx targety jump fire hook playerflags pos_x pos_y vel_x vel_y\n";
	
	for(const auto &Frame : m_vRecording)
	{
		File << Frame.m_Tick << " "
			 << Frame.m_Input.m_Direction << " "
			 << Frame.m_Input.m_TargetX << " "
			 << Frame.m_Input.m_TargetY << " "
			 << Frame.m_Input.m_Jump << " "
			 << Frame.m_Input.m_Fire << " "
			 << Frame.m_Input.m_Hook << " "
			 << Frame.m_Input.m_PlayerFlags << " "
			 << Frame.m_Position.x << " "
			 << Frame.m_Position.y << " "
			 << Frame.m_Velocity.x << " "
			 << Frame.m_Velocity.y << "\n";
	}
	
	File.close();
}

bool CTASWorld::LoadFromFile(const char *pFilename)
{
	std::ifstream File(pFilename);
	if(!File.is_open())
		return false;
		
	m_vRecording.clear();
	std::string Line;
	
	while(std::getline(File, Line))
	{
		if(Line.empty() || Line[0] == '#')
			continue;
			
		std::istringstream iss(Line);
		CTASFrame Frame;
		
		if(iss >> Frame.m_Tick
			   >> Frame.m_Input.m_Direction
			   >> Frame.m_Input.m_TargetX
			   >> Frame.m_Input.m_TargetY
			   >> Frame.m_Input.m_Jump
			   >> Frame.m_Input.m_Fire
			   >> Frame.m_Input.m_Hook
			   >> Frame.m_Input.m_PlayerFlags
			   >> Frame.m_Position.x
			   >> Frame.m_Position.y
			   >> Frame.m_Velocity.x
			   >> Frame.m_Velocity.y)
		{
			m_vRecording.push_back(Frame);
		}
	}
	
	File.close();
	return true;
}

// CTAS реализация
CTAS::CTAS()
{
	m_Active = false;
	m_ShowTASMenu = false;
	str_copy(m_aFilename, "recording.tas", sizeof(m_aFilename));
}

void CTAS::OnInit()
{
	// Инициализация TAS системы
}

void CTAS::OnReset()
{
	m_TASWorld.Reset();
	m_Active = false;
}

void CTAS::OnRender()
{
	if(!m_Active)
		return;
		
	RenderTASInfo();
	RenderTASCharacter();
	
	if(m_ShowTASMenu)
		RenderTASMenu();
}

void CTAS::OnMessage(int MsgType, void *pRawMsg)
{
	// Обработка сетевых сообщений если нужно
}

bool CTAS::OnInput(const IInput::CEvent &Event)
{
	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		// Горячие клавиши для TAS
		if(Event.m_Key == KEY_F9)
		{
			m_ShowTASMenu = !m_ShowTASMenu;
			return true;
		}
		
		if(Event.m_Key == KEY_F10)
		{
			if(m_TASWorld.IsRecording())
				StopRecording();
			else
				StartRecording();
			return true;
		}
		
		if(Event.m_Key == KEY_F11)
		{
			if(m_TASWorld.IsReplaying())
				StopReplay();
			else
				StartReplay();
			return true;
		}
		
		if(Event.m_Key == KEY_F12)
		{
			if(m_TASWorld.IsPaused())
				Resume();
			else
				Pause();
			return true;
		}
	}
	
	return false;
}

void CTAS::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE)
	{
		// Инициализируем TAS мир при подключении к серверу
		CCharacter *pCharacter = m_pClient->m_GameWorld.GetCharacterById(m_pClient->m_aLocalIds[0]);
		if(pCharacter)
		{
			m_TASWorld.Init(&m_pClient->m_GameWorld, pCharacter);
			m_Active = true;
		}
	}
	else if(NewState == IClient::STATE_OFFLINE)
	{
		m_TASWorld.Reset();
		m_Active = false;
	}
}

void CTAS::StartRecording()
{
	m_TASWorld.StartRecording();
}

void CTAS::StopRecording()
{
	m_TASWorld.StopRecording();
}

void CTAS::StartReplay()
{
	m_TASWorld.StartReplay();
}

void CTAS::StopReplay()
{
	m_TASWorld.StopReplay();
}

void CTAS::Pause()
{
	m_TASWorld.Pause();
}

void CTAS::Resume()
{
	m_TASWorld.Resume();
}

void CTAS::SaveRecording(const char *pFilename)
{
	m_TASWorld.SaveToFile(pFilename);
}

void CTAS::LoadRecording(const char *pFilename)
{
	m_TASWorld.LoadFromFile(pFilename);
}

void CTAS::RenderTASMenu()
{
	// Простое меню TAS (будет улучшено в следующих версиях)
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	
	CUIRect Screen = *m_pClient->UI()->Screen();
	CUIRect Box;
	Screen.HSplitMid(&Box, nullptr);
	Box.VSplitMid(&Box, nullptr);
	Box.w = 300.0f;
	Box.h = 200.0f;
	
	m_pClient->RenderTools()->DrawUIRect(&Box, ColorRGBA(0, 0, 0, 0.8f), IGraphics::CORNER_ALL, 5.0f);
	
	Box.Margin(10.0f, &Box);
	
	// Заголовок
	CUIRect Title;
	Box.HSplitTop(20.0f, &Title, &Box);
	m_pClient->UI()->DoLabel(&Title, "TAS System", 16.0f, TEXTALIGN_MC);
	
	Box.HSplitTop(10.0f, nullptr, &Box);
	
	// Кнопки
	CUIRect Button;
	Box.HSplitTop(20.0f, &Button, &Box);
	static CButtonContainer s_RecordButton;
	if(m_pClient->UI()->DoButton_Menu(&s_RecordButton, m_TASWorld.IsRecording() ? "Stop Recording" : "Start Recording", 0, &Button))
	{
		if(m_TASWorld.IsRecording())
			StopRecording();
		else
			StartRecording();
	}
	
	Box.HSplitTop(5.0f, nullptr, &Box);
	Box.HSplitTop(20.0f, &Button, &Box);
	static CButtonContainer s_ReplayButton;
	if(m_pClient->UI()->DoButton_Menu(&s_ReplayButton, m_TASWorld.IsReplaying() ? "Stop Replay" : "Start Replay", 0, &Button))
	{
		if(m_TASWorld.IsReplaying())
			StopReplay();
		else
			StartReplay();
	}
	
	Box.HSplitTop(5.0f, nullptr, &Box);
	Box.HSplitTop(20.0f, &Button, &Box);
	static CButtonContainer s_PauseButton;
	if(m_pClient->UI()->DoButton_Menu(&s_PauseButton, m_TASWorld.IsPaused() ? "Resume" : "Pause", 0, &Button))
	{
		if(m_TASWorld.IsPaused())
			Resume();
		else
			Pause();
	}
	
	Box.HSplitTop(5.0f, nullptr, &Box);
	Box.HSplitTop(20.0f, &Button, &Box);
	static CButtonContainer s_SaveButton;
	if(m_pClient->UI()->DoButton_Menu(&s_SaveButton, "Save Recording", 0, &Button))
	{
		SaveRecording(m_aFilename);
	}
	
	Box.HSplitTop(5.0f, nullptr, &Box);
	Box.HSplitTop(20.0f, &Button, &Box);
	static CButtonContainer s_LoadButton;
	if(m_pClient->UI()->DoButton_Menu(&s_LoadButton, "Load Recording", 0, &Button))
	{
		LoadRecording(m_aFilename);
	}
}

void CTAS::RenderTASInfo()
{
	// Отображение информации о TAS
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "TAS: Tick %d | Recording: %s | Replaying: %s | Frames: %d", 
		m_TASWorld.GetCurrentTick(),
		m_TASWorld.IsRecording() ? "ON" : "OFF",
		m_TASWorld.IsReplaying() ? "ON" : "OFF",
		m_TASWorld.GetRecordingSize());
		
	TextRender()->Text(10.0f, 10.0f, 12.0f, aBuf, -1.0f);
}

void CTAS::RenderTASCharacter()
{
	// Отрисовка TAS персонажа (ghost)
	CTASCharacter *pTASChar = m_TASWorld.GetTASCharacter();
	if(!pTASChar)
		return;
		
	// Простая отрисовка позиции TAS персонажа
	vec2 ScreenPos = vec2(pTASChar->m_Pos.x, pTASChar->m_Pos.y);
	
	// Конвертируем мировые координаты в экранные
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	
	vec2 Center = m_pClient->m_Camera.m_Center;
	float Zoom = m_pClient->m_Camera.m_Zoom;
	
	ScreenPos.x = (ScreenPos.x - Center.x) / Zoom + Graphics()->ScreenWidth() / 2.0f;
	ScreenPos.y = (ScreenPos.y - Center.y) / Zoom + Graphics()->ScreenHeight() / 2.0f;
	
	// Рисуем простой круг для TAS персонажа
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.5f); // Красный полупрозрачный
	IGraphics::CQuadItem QuadItem(ScreenPos.x - 16, ScreenPos.y - 16, 32, 32);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

