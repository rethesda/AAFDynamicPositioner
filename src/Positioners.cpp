#include "Positioners.h"

#include "Scaleforms.h"
#include "PositionData.h"
#include "Utils.h"

namespace Positioners {
	struct SceneData {
		std::uint64_t              SceneID;
		std::string                Position;
		std::vector<std::uint32_t> ActorList;
	};

	enum POSITIONER_TYPE : std::uint32_t {
		kRelative = 0,
		kAbsolute
	};

	std::uint32_t g_playerPositionerType = POSITIONER_TYPE::kRelative;
	std::uint32_t g_npcPositionerType = POSITIONER_TYPE::kRelative;

	enum CAN_MOVE : std::uint32_t {
		kYes = 0,
		kNo_Selection,
		kNo_Scale,
	};

	std::unordered_map<std::uint32_t, ActorData> g_actorMap;
	std::unordered_map<std::uint64_t, SceneData> g_sceneMap;

	bool g_separatePlayerOffset = false;
	bool g_unifyAAFDoppelgangerScale = true;
	std::uint64_t g_sceneMapKey = 1;
	std::uint32_t g_selectedActorFormID = 0;

	std::uint32_t GetSelectedActorFormID() {
		return g_selectedActorFormID;
	}

	void SetSelectedActorFormID(std::uint32_t a_formID) {
		g_selectedActorFormID = a_formID;
	}

	void ClearSelectedActorFormID() {
		SetSelectedActorFormID(0);
	}

	ActorData* GetActorDataByFormID(std::uint32_t a_formID) {
		auto it = g_actorMap.find(a_formID);
		if (it == g_actorMap.end()) {
			return nullptr;
		}

		return &it->second;
	}

	ActorData* GetPlayerActorData() {
		RE::PlayerCharacter* g_player = RE::PlayerCharacter::GetSingleton();
		if (!g_player) {
			return nullptr;
		}

		return GetActorDataByFormID(g_player->formID);
	}

	ActorData* GetSelectedActorData() {
		if (!GetSelectedActorFormID()) {
			return nullptr;
		}

		return GetActorDataByFormID(GetSelectedActorFormID());
	}

	SceneData* GetSceneDataByID(std::uint64_t a_sceneID) {
		auto it = g_sceneMap.find(a_sceneID);
		if (it == g_sceneMap.end()) {
			return nullptr;
		}

		return &it->second;
	}

	std::uint64_t GetSceneIDFromActorList(const RE::BSTArray<RE::Actor*>& a_actorList) {
		for (auto actor : a_actorList) {
			ActorData* actorData = GetActorDataByFormID(actor->formID);
			if (!actorData) {
				continue;
			}

			// 액터를 이용하여 씬의 ID를 가져옴
			return actorData->SceneID;
		}

		return 0;
	}

	ExtraRefrPath* GetExtraRefrPath(RE::Actor* a_actor) {
		if (!a_actor) {
			return nullptr;
		}

		RE::BSExtraData* refrPath = a_actor->extraList->extraData.GetByType(RE::EXTRA_DATA_TYPE::kRefrPath);
		if (!refrPath) {
			return nullptr;
		}

		return (ExtraRefrPath*)refrPath;
	}

	RE::SpellItem* GetHighlightSpell(bool a_isMovable) {
		static RE::SpellItem* movableSpell = nullptr;
		static RE::SpellItem* immovableSpell = nullptr;

		if (a_isMovable) {
			if (!movableSpell) {
				auto movableSpellForm = Utils::GetFormFromIdentifier("AAFDynamicPositioner.esp"sv, 0x00000810);
				if (movableSpellForm) {
					movableSpell = movableSpellForm->As<RE::SpellItem>();
				}
			}
			return movableSpell;
		}
		else {
			if (!immovableSpell) {
				auto immovableSpellForm = Utils::GetFormFromIdentifier("AAFDynamicPositioner.esp"sv, 0x00000811);
				if (immovableSpellForm) {
					immovableSpell = immovableSpellForm->As<RE::SpellItem>();
				}
			}
			return immovableSpell;
		}
	}

	void ClearHighlightSpellFromActor(RE::Actor* a_actor) {
		if (Utils::HasSpell(a_actor, GetHighlightSpell(true))) {
			Utils::RemoveSpell(a_actor, GetHighlightSpell(true));
		}
		if (Utils::HasSpell(a_actor, GetHighlightSpell(false))) {
			Utils::RemoveSpell(a_actor, GetHighlightSpell(false));
		}
	}

	bool IsPlayerInScene(SceneData* a_sceneData) {
		if (!a_sceneData) {
			return false;
		}

		ActorData* playerActorData = GetPlayerActorData();
		if (!playerActorData) {
			return false;
		}

		for (auto formId : a_sceneData->ActorList) {
			if (formId == playerActorData->FormID) {
				return true;
			}
		}

		return false;
	}

	bool IsActorInPlayerScene(ActorData* a_actorData) {
		if (!a_actorData) {
			return false;
		}

		ActorData* playerActorData = GetPlayerActorData();
		if (!playerActorData) {
			return false;
		}

		if (a_actorData->SceneID == playerActorData->SceneID) {
			return true;
		}

		return false;
	}

	bool IsActorScale1(RE::Actor* a_actor) {
		if (!a_actor) {
			return false;
		}

		float actorScale = Utils::GetActualScale(a_actor);
		std::uint32_t intScale = static_cast<std::uint32_t>(std::round(actorScale * 100));
		if (intScale == 100) {
			return true;
		}
		return false;
	}

	std::vector<PositionData::Data> LoadPosition(SceneData* a_sceneData) {
		if (!a_sceneData) {
			return std::vector<PositionData::Data>();
		}

		return PositionData::LoadPositionData(a_sceneData->Position, g_separatePlayerOffset ? IsPlayerInScene(a_sceneData) : false);
	}

	void SavePosition(SceneData* a_sceneData) {
		if (!a_sceneData) {
			return;
		}

		PositionData::SavePositionData(a_sceneData->Position, a_sceneData->ActorList, g_separatePlayerOffset ? IsPlayerInScene(a_sceneData) : false);
	}

	void SavePosition(ActorData* a_actorData) {
		if (!a_actorData) {
			return;
		}

		SavePosition(GetSceneDataByID(a_actorData->SceneID));
	}

	std::vector<PositionData::Data> GetPreviousPosition(SceneData* a_sceneData) {
		std::vector<PositionData::Data> retVec;

		if (!a_sceneData) {
			return retVec;
		}

		for (auto actorFormID : a_sceneData->ActorList) {
			auto actorData = GetActorDataByFormID(actorFormID);
			if (!actorData) {
				continue;
			}

			PositionData::Data nData;
			nData.index = actorData->PositionIndex;
			nData.offset = actorData->Offset;
			retVec.push_back(nData);
		}

		return retVec;
	}

	std::optional<PositionData::Data> GetPositionData(const std::vector<PositionData::Data> a_posVec, std::uint32_t a_posIdx) {
		for (PositionData::Data data : a_posVec) {
			if (data.index == a_posIdx) {
				return data;
			}
		}

		return std::nullopt;
	}

	RE::NiPoint3 GetOffsetFromPositionData(const std::vector<PositionData::Data> a_posVec, std::uint32_t a_posIdx) {
		auto data = GetPositionData(a_posVec, a_posIdx);
		if (!data.has_value()) {
			return RE::NiPoint3{};
		}
		return data->offset;
	}

	RE::NiPoint3 GetRotatedOffset(const RE::NiPoint3& a_offset, float a_rot) {
		float rotatedOffsetX = a_offset.x * cos(a_rot) + a_offset.y * sin(a_rot);
		float rotatedOffsetY = -a_offset.x * sin(a_rot) + a_offset.y * cos(a_rot);
		float rotatedOffsetZ = a_offset.z;
		return RE::NiPoint3(rotatedOffsetX, rotatedOffsetY, rotatedOffsetZ);
	}

	void ApplyOffset(ActorData* a_actorData) {
		std::uint32_t positionerType = IsActorInPlayerScene(a_actorData) ? g_playerPositionerType : g_npcPositionerType;
		if (positionerType == POSITIONER_TYPE::kRelative && IsActorScale1(a_actorData->Actor)) {
			return;
		}

		RE::NiPoint3 rotatedOffset = GetRotatedOffset(a_actorData->Offset, a_actorData->Actor->data.angle.z);

		if (a_actorData->ExtraRefrPath) {
			if (positionerType == POSITIONER_TYPE::kRelative) {
				float scale = Utils::GetActualScale(a_actorData->Actor);
				float scaleDiff = 1.0f - scale;
				a_actorData->ExtraRefrPath->goalPos.x = a_actorData->OriginalPosition.x + rotatedOffset.x * scaleDiff;
				a_actorData->ExtraRefrPath->goalPos.y = a_actorData->OriginalPosition.y + rotatedOffset.y * scaleDiff;
				a_actorData->ExtraRefrPath->goalPos.z = a_actorData->OriginalPosition.z + rotatedOffset.z * scaleDiff;
			}
			else if (positionerType == POSITIONER_TYPE::kAbsolute) {
				a_actorData->ExtraRefrPath->goalPos.x = a_actorData->OriginalPosition.x + rotatedOffset.x;
				a_actorData->ExtraRefrPath->goalPos.y = a_actorData->OriginalPosition.y + rotatedOffset.y;
				a_actorData->ExtraRefrPath->goalPos.z = a_actorData->OriginalPosition.z + rotatedOffset.z;
			}

			Utils::ModPos(a_actorData->Actor, 'X', a_actorData->ExtraRefrPath->goalPos.x);
			Utils::ModPos(a_actorData->Actor, 'Y', a_actorData->ExtraRefrPath->goalPos.y);
			Utils::ModPos(a_actorData->Actor, 'Z', a_actorData->ExtraRefrPath->goalPos.z);
		}
	}

	void SetOffset(const std::string& a_axis, float a_offset) {
		ActorData* actorData = GetSelectedActorData();
		if (!actorData) {
			return;
		}

		ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->Actor);
		if (!extraRefPath) {
			return;
		}

		if (actorData->ExtraRefrPath != extraRefPath) {
			actorData->ExtraRefrPath = extraRefPath;
			actorData->OriginalPosition = extraRefPath->goalPos;
		}

		if (a_axis == "X") {
			actorData->Offset.x = a_offset;
		}
		else if (a_axis == "Y") {
			actorData->Offset.y = a_offset;
		}
		else if (a_axis == "Z") {
			actorData->Offset.z = a_offset;
		}

		ApplyOffset(actorData);
		SavePosition(actorData);
	}

	void ClearOffset() {
		ActorData* actorData = GetSelectedActorData();
		if (!actorData) {
			return;
		}

		ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->Actor);
		if (!extraRefPath) {
			return;
		}

		if (actorData->ExtraRefrPath != extraRefPath) {
			actorData->ExtraRefrPath = extraRefPath;
			actorData->OriginalPosition = extraRefPath->goalPos;
		}

		actorData->Offset = RE::NiPoint3{};

		ApplyOffset(actorData);
		SavePosition(actorData);
	}

	void ResetPositioner() {
		g_actorMap.clear();
		g_sceneMap.clear();
		g_sceneMapKey = 1;
		ClearSelectedActorFormID();
	}

	void SceneInit(std::monostate, RE::BSTArray<RE::Actor*> a_actors, RE::Actor* a_doppelganger) {
		SceneData newScene;

		// 새 씬 초기화
		newScene.SceneID = g_sceneMapKey++;

		RE::PlayerCharacter* g_player = RE::PlayerCharacter::GetSingleton();

		for (auto actor : a_actors) {
			RE::Actor* actorPtr = actor;

			bool isPlayerActor = false;

			// 현재 액터가 플레이어인 경우 인자의 도플갱어를 사용
			if (actorPtr == g_player) {
				isPlayerActor = true;

				actorPtr = a_doppelganger;
				if (!actorPtr) {
					continue;
				}

				if (g_unifyAAFDoppelgangerScale) {
					Utils::SetRefScale(actorPtr, Utils::GetActualScale(g_player));
				}
			}

			// 액터 정보 초기화
			ActorData actorData;
			actorData.FormID = isPlayerActor ? g_player->formID : actorPtr->formID;
			actorData.Actor = actorPtr;
			actorData.SceneID = newScene.SceneID;
			actorData.ExtraRefrPath = nullptr;
			actorData.Offset = RE::NiPoint3();
			actorData.OriginalPosition = RE::NiPoint3();

			// 초기화한 액터 정보를 씬의 액터 리스트에 삽입
			newScene.ActorList.push_back(actorData.FormID);

			// 씬의 액터 리스트에 삽입된 액터 정보의 포인터를 액터 맵에 추가함
			g_actorMap.insert(std::make_pair(actorData.FormID, actorData));
		}

		// 씬을 씬 맵에 삽입
		g_sceneMap.insert(std::make_pair(newScene.SceneID, newScene));
	}

	void AnimationChange(std::monostate, std::string a_position, RE::BSTArray<RE::Actor*> a_actors) {
		std::uint64_t sceneID = GetSceneIDFromActorList(a_actors);
		if (!sceneID) {
			return;
		}

		// 씬 ID를 이용하여 씬 맵에서 씬을 찾는다
		SceneData* sceneData = GetSceneDataByID(sceneID);
		if (!sceneData) {
			return;
		}

		std::vector<PositionData::Data> prevPosDataVec = GetPreviousPosition(sceneData);
		sceneData->Position = a_position;

		// 씬 정보를 이용하여 위치 정보를 읽어옴
		std::vector<PositionData::Data> posDataVec = LoadPosition(sceneData);

		for (std::uint32_t ii = 0; ii < a_actors.size(); ii++) {
			RE::Actor* actorPtr = a_actors[ii];

			ActorData* actorData = GetActorDataByFormID(actorPtr->formID);
			if (!actorData) {
				continue;
			}

			actorData->PositionIndex = ii;
			if (!posDataVec.empty()) {
				actorData->Offset = GetOffsetFromPositionData(posDataVec, ii);
			}
			else {
				actorData->Offset = GetOffsetFromPositionData(prevPosDataVec, ii);
			}

			if (Scaleforms::IsMenuOpen() && actorData->FormID == GetSelectedActorFormID()) {
				Scaleforms::UpdateMenu(actorData->Offset);
			}

			// 액터의 실제 위치를 저장하는 ExtraRefrPath를 불러옴
			ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->Actor);
			if (!actorData->ExtraRefrPath && !extraRefPath) {
				continue;
			}

			// ExtraRefPath는 변하지 않은 경우
			if (actorData->ExtraRefrPath && actorData->ExtraRefrPath == extraRefPath) {
				// 위치 조절 전 좌표로 원복
				actorData->ExtraRefrPath->goalPos = actorData->OriginalPosition;
			}
			// ExtraRefPath가 변한 경우
			else {
				actorData->ExtraRefrPath = extraRefPath;
				if (extraRefPath) {
					actorData->OriginalPosition = extraRefPath->goalPos;
				}
			}

			// 액터 오프셋 적용
			ApplyOffset(actorData);
		}
	}

	void SceneEnd(std::monostate, RE::BSTArray<RE::Actor*> a_actors) {
		std::uint64_t sceneId = GetSceneIDFromActorList(a_actors);
		if (!sceneId) {
			return;
		}

		for (auto actor : a_actors) {
			ActorData* actorData = GetActorDataByFormID(actor->formID);
			if (!actorData) {
				continue;
			}

			// 선택한 액터가 종료되는 씬에 포함되어있을 경우 선택한 액터를 초기화
			if (actorData->FormID == GetSelectedActorFormID()) {
				if (Scaleforms::IsMenuOpen()) {
					Scaleforms::CloseMenu();
				}

				ClearHighlightSpellFromActor(actor);
				ClearSelectedActorFormID();
			}

			g_actorMap.erase(actorData->FormID);
		}

		g_sceneMap.erase(sceneId);
	}

	std::uint32_t CanMovePosition(std::monostate) {
		ActorData* selectedActorData = GetSelectedActorData();
		if (!selectedActorData) {
			return CAN_MOVE::kNo_Selection;
		}

		std::uint32_t positionerType = IsActorInPlayerScene(selectedActorData) ? g_playerPositionerType : g_npcPositionerType;

		// 위치 조절 타입이 스케일이고 액터 스케일이 1이면 이동 불가
		if (positionerType == POSITIONER_TYPE::kRelative && IsActorScale1(selectedActorData->Actor)) {
			return CAN_MOVE::kNo_Scale;
		}

		return CAN_MOVE::kYes;
	}

	RE::Actor* ChangeSelectedActor() {
		ActorData* actorData;
		SceneData* sceneData;

		// 현재 선택되어있는 액터를 가져옴
		actorData = GetSelectedActorData();

		// 현재 선택되어있는 액터가 없을 경우
		if (!actorData) {
			// 플레이어로 진행중인 씬이 있는지 확인
			actorData = GetPlayerActorData();
			if (actorData) {
				// 플레이어로 진행중인 씬이 있을 때 씬 정보를 가져옴
				SceneData* playerScene = GetSceneDataByID(actorData->SceneID);
				if (!playerScene || playerScene->ActorList.empty()) {
					return nullptr;
				}

				// 해당 씬의 가장 첫 액터를 선택하여 반환
				actorData = GetActorDataByFormID(playerScene->ActorList.front());
				if (!actorData) {
					return nullptr;
				}

				SetSelectedActorFormID(actorData->FormID);
				return actorData->Actor;
			}

			// 플레이어로 진행중인 씬이 없는 경우
			// 첫 씬의 첫 액터를 선택하여 반환함
			if (g_sceneMap.empty()) {
				return nullptr;
			}

			sceneData = &g_sceneMap.begin()->second;
			if (sceneData->ActorList.empty()) {
				return nullptr;
			}

			actorData = GetActorDataByFormID(sceneData->ActorList.front());
			if (!actorData) {
				return nullptr;
			}

			SetSelectedActorFormID(actorData->FormID);
			return actorData->Actor;
		}

		// 현재 선택되어있는 액터가 있을 경우 현재 액터가 포함된 씬을 가져옴
		auto scene_iter = g_sceneMap.find(actorData->SceneID);
		if (scene_iter == g_sceneMap.end()) {
			return nullptr;
		}

		sceneData = &scene_iter->second;

		// 현재 선택된 액터를 찾고 그 액터의 다음 액터를 찾아서 반환
		auto actor_iter = std::find(sceneData->ActorList.begin(), sceneData->ActorList.end(), actorData->FormID);
		if (actor_iter == sceneData->ActorList.end()) {
			return nullptr;
		}

		actor_iter = std::next(actor_iter);
		if (actor_iter != sceneData->ActorList.end()) {
			actorData = GetActorDataByFormID(*actor_iter);
			if (!actorData) {
				return nullptr;
			}

			SetSelectedActorFormID(actorData->FormID);
			return actorData->Actor;
		}

		// 선택된 액터가 해당 씬의 마지막 액터였을 경우
		// 해당 씬의 다음 씬의 첫 액터를 찾아서 반환
		scene_iter = std::next(scene_iter);
		if (scene_iter != g_sceneMap.end()) {
			sceneData = &scene_iter->second;
			if (sceneData->ActorList.empty()) {
				return nullptr;
			}

			actorData = GetActorDataByFormID(sceneData->ActorList.front());
			if (!actorData) {
				return nullptr;
			}

			SetSelectedActorFormID(actorData->FormID);
			return actorData->Actor;
		}

		// 선택된 액터가 마지막 씬의 마지막 액터였을 경우
		// 가장 첫 씬의 첫 액터를 찾아서 반환
		sceneData = &g_sceneMap.begin()->second;
		if (sceneData->ActorList.empty()) {
			return nullptr;
		}

		actorData = GetActorDataByFormID(sceneData->ActorList.front());
		if (!actorData) {
			return nullptr;
		}

		SetSelectedActorFormID(actorData->FormID);
		return actorData->Actor;
	}

	bool ChangeActor_Native(std::monostate) {
		ActorData* selectedActorData = GetSelectedActorData();
		if (selectedActorData) {
			ClearHighlightSpellFromActor(selectedActorData->Actor);
		}

		RE::Actor* selectedActor = ChangeSelectedActor();
		if (!selectedActor) {
			return false;
		}

		if (CanMovePosition({}) == CAN_MOVE::kYes) {
			Utils::AddSpell(selectedActor, GetHighlightSpell(true));
		}
		else {
			Utils::AddSpell(selectedActor, GetHighlightSpell(false));
		}

		return true;
	}

	void ClearActorSelection(std::monostate) {
		ActorData* selectedActorData = GetSelectedActorData();
		if (selectedActorData) {
			ClearHighlightSpellFromActor(selectedActorData->Actor);
		}
		ClearSelectedActorFormID();
	}

	void ShowPositionerMenu_Native(std::monostate) {
		ActorData* actorData = GetSelectedActorData();
		if (!actorData) {
			return;
		}

		ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->Actor);
		if (!extraRefPath) {
			return;
		}

		if (actorData->ExtraRefrPath != extraRefPath) {
			actorData->ExtraRefrPath = extraRefPath;
			actorData->OriginalPosition = extraRefPath->goalPos;
		}

		if (Scaleforms::IsMenuOpen()) {
			return;
		}

		Scaleforms::OpenMenu(actorData->Offset);
	}

	void Install(RE::BSScript::IVirtualMachine* a_vm) {
		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "SceneInit"sv, SceneInit);
		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "AnimationChange"sv, AnimationChange);
		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "SceneEnd"sv, SceneEnd);

		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "CanMovePosition"sv, CanMovePosition);
		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "ChangeActor_Native"sv, ChangeActor_Native);
		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "ClearActorSelection"sv, ClearActorSelection);

		a_vm->BindNativeMethod("AAFDynamicPositioner"sv, "ShowPositionerMenu_Native"sv, ShowPositionerMenu_Native);
	}
}
