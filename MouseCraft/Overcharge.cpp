#include "Overcharge.h"

Overcharge::Overcharge()
{
	Contraption::type = CONTRAPTIONS::OVERCHARGE;
}


Overcharge::~Overcharge()
{
}

bool Overcharge::use(Mouse* m) {
	std::cout << "OVERCHARGE is being used" << std::endl;
	this->GetEntity()->GetParent()->GetComponent<HealthComponent>()->AddShield();
	this->Drop();
	this->GetEntity()->Destroy();

	return true;
}

void Overcharge::show() {
	Contraption::show();
}