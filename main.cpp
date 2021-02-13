#include <glv/viewer.h>
#include <glv/first_person_manip.h>
#include <glv/iapplication.h>
#include <app/application.h>

int main(int argc, char** argv)
{
	app::application a;
	glv::viewer::set_application(&a);
	glv::first_person_manip first_person;
	glv::viewer::set_camera_manip(&first_person);
	return glv::viewer::exec(argc, argv);
}
