#include <Nazara/Platform.hpp>
#include <Nazara/Utility.hpp>

int main()
{
	Nz::Initializer<Nz::Platform> nazara;

	if (!nazara.IsInitialized())
		return -1;

	Nz::Window window(Nz::VideoMode(400, 300), "Menu");

	Nz::Menu menu;

	Nz::MenuButton& button = menu.AppendButton("Button");
	menu.AppendSeparator();

	button.OnPressed.Connect([](Nz::MenuButton& btn)
		{
			btn.SetEnabled(false);
			Nz::Log::Write("Button Disabled");
		}
	);

	Nz::SubMenu& subMenu = menu.AppendSubMenu("SubMenu");

	Nz::MenuButton& subButton = subMenu.AppendButton("Toggle");
	subMenu.AppendSeparator();

	subButton.OnPressed.Connect([&](Nz::MenuButton& btn)
		{
			btn.SetChecked(!btn.IsChecked());
			btn.SetName("Toggle " + std::to_string(btn.IsChecked()));
			Nz::Log::Write(btn.GetName());
			window.ForceDrawMenu();
		}
	);

	Nz::MenuButton& enableButton = subMenu.AppendButton("Enable");

	enableButton.OnPressed.Connect([&](Nz::MenuButton&)
		{
			button.SetEnabled(true);
			Nz::Log::Write("Button Enabled");
			window.ForceDrawMenu();
		}
	);

	Nz::MenuButton& enableButton2 = menu.AppendButton("Enable");

	enableButton2.OnPressed.Connect([&](Nz::MenuButton&)
		{
			button.SetEnabled(true);
			Nz::Log::Write("Button Enabled");
			window.ForceDrawMenu();
		}
	);

	window.SetMenu(menu);

	while (window.IsOpen())
	{
		window.ProcessEvents();
	}
	
	return 0;
}
