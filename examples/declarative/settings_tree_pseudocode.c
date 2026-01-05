/*
 * Declarative tree sketch for 9de-settings (pseudo-code).
 * Goal: show structure, not compile today.
 */

UiNode *SettingsApp(void){
  return uiVStack(8,
    uiHeader("System Settings", "9DE shell-first"),
    uiSplit(260,
      uiSidebar(
        uiNavItem("Appearance"),
        uiNavItem("Panel"),
        uiNavItem("Session"),
        uiNavItem("Services")
      ),
      uiSurface(
        uiVStack(0,
          uiRowGroup("Appearance"),
          uiRowSelect("Preset", uiOptions("terminal","dark","glass")),
          uiRowSlider("Alpha", 0, 255),
          uiRowGroup("Panel"),
          uiRowSelect("Placement", uiOptions("top","bottom","left")),
          uiRowModules("Modules", uiModules("9de","ws","clock","net","vol"))
        )
      )
    )
  );
}
