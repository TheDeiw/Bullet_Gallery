#include "GuardAIController.h"

AGuardAIController::AGuardAIController()
{
	// Without a focus the control rotation follows the body, so the guard's eyes look where the body faces
	bSetControlRotationFromPawnOrientation = true;
}
