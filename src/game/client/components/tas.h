#ifndef GAME_CLIENT_COMPONENTS_TAS_H
#define GAME_CLIENT_COMPONENTS_TAS_H

#include <base/vmath.h>
#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/prediction/entities/character.h>
#include <game/generated/protocol.h>
#include <vector>
#include <string>

// Структура для хранения TAS фрейма
struct CTASFrame
{
	int m_Tick;
	CNetObj_PlayerInput m_Input;
	vec2 m_Position;
	vec2 m_Velocity;
	int m_Flags;
	
	CTASFrame() : m_Tick(0), m_Flags(0) 
	{
		mem_zero(&m_Input, sizeof(m_Input));
		m_Position = vec2(0, 0);
		m_Velocity = vec2(0, 0);
	}
};

// TAS персонаж - изолированная копия персонажа для симуляции
class CTASCharacter : public CCharacter
{
public:
	CTASCharacter(CGameWorld *pGameWorld, int ID, CNetObj_Character *pChar);
	
	void ApplyInput(CNetObj_PlayerInput *pInput);
	void SetInput(CNetObj_PlayerInput *pInput) { m_Input = *pInput; }
	
	// Переопределяем методы для изоляции от сети
	// virtual void Snap(int SnappingClient) override {} // Не отправляем снапы
	// virtual void PostSnap() override {} // Не обрабатываем снапы
	
private:
	CNetObj_PlayerInput m_Input;
};

// TAS мир - изолированная копия игрового мира
class CTASWorld
{
private:
	CGameWorld *m_pGameWorld;
	CTASCharacter *m_pTASCharacter;
	std::vector<CTASFrame> m_vRecording;
	
	bool m_Recording;
	bool m_Replaying;
	bool m_Paused;
	int m_CurrentTick;
	int m_ReplayIndex;
	int m_TPS; // Ticks per second для воспроизведения
	
public:
	CTASWorld();
	~CTASWorld();
	
	void Init(CGameWorld *pGameWorld, CCharacter *pCharacter);
	void Reset();
	
	// Основные функции TAS
	void StartRecording();
	void StopRecording();
	void StartReplay();
	void StopReplay();
	void Pause();
	void Resume();
	
	// Обновление симуляции
	void Tick();
	void RecordFrame(CNetObj_PlayerInput *pInput);
	bool ReplayFrame();
	
	// Сохранение/загрузка
	void SaveToFile(const char *pFilename);
	bool LoadFromFile(const char *pFilename);
	
	// Геттеры
	bool IsRecording() const { return m_Recording; }
	bool IsReplaying() const { return m_Replaying; }
	bool IsPaused() const { return m_Paused; }
	int GetCurrentTick() const { return m_CurrentTick; }
	int GetRecordingSize() const { return m_vRecording.size(); }
	int GetTPS() const { return m_TPS; }
	CTASCharacter *GetTASCharacter() { return m_pTASCharacter; }
	
	// Сеттеры
	void SetTPS(int TPS) { m_TPS = TPS; }
};

// Основной компонент TAS системы
class CTAS : public CComponent
{
private:
	CTASWorld m_TASWorld;
	bool m_Active;
	
	// UI состояние
	bool m_ShowTASMenu;
	char m_aFilename[256];
	
	// Для UI кнопок
	struct CButtonContainer {};
	
public:
	CTAS();
	
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual bool OnInput(const IInput::CEvent &Event) override;
	virtual void OnStateChange(int NewState, int OldState) override;
	
	// TAS функции
	void StartRecording();
	void StopRecording();
	void StartReplay();
	void StopReplay();
	void Pause();
	void Resume();
	
	void SaveRecording(const char *pFilename);
	void LoadRecording(const char *pFilename);
	
	// UI
	void RenderTASMenu();
	void RenderTASInfo();
	void RenderTASCharacter();
	
	// Геттеры
	bool IsActive() const { return m_Active; }
	CTASWorld *GetTASWorld() { return &m_TASWorld; }
};

#endif

