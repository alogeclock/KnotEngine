#pragma once

#include "Input/InputRouter.h"
#include "ViewportClient.h"

class FLevelEditorViewportClient : public FViewportClient, public IInputTarget
{
public:
	FInputReply OnInputEvent(const FInputEvent& Event) override;
};
