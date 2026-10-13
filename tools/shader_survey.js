// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs (tools/shader_plan.json): finds the shader
// survey struct of patches/shader_survey.c in RDRAM by its magic words, and offers surveyMenu(menu, stage), which asks
// the game to show a menu with that stage's briefing behind it (MENU_RUN_STAGE starts the stage) and resolves once it
// has taken the request, and setCursor(x, y), which moves the menus' cursor (cursor_h_pos and cursor_v_pos, floats).
(() => {
  if (!window.__survey) {
    for (let a = 0x80000000; a < 0x80c00000; a += 4) {
      if (n64r(a) === 0x53484452 && n64r(a + 4) === 0x53525659) { window.__survey = a; break; }
    }
  }
  const s = window.__survey;
  window.surveyMenu = async (menu, stage = -1) => {
    const before = n64r(s + 8);
    n64w(s + 16, stage);
    n64w(s + 12, menu);
    for (let i = 0; i < 200 && n64r(s + 8) === before; i++) await new Promise((resolve) => setTimeout(resolve, 50));
    return n64r(s + 8) !== before;
  };
  const bits = new DataView(new ArrayBuffer(4));
  const float = (v) => { bits.setFloat32(0, v); return bits.getUint32(0); };
  window.setCursor = (x, y) => { n64w(0x8002A708, float(x)); n64w(0x8002A70C, float(y)); return true; };
  return s ? 'survey at ' + s.toString(16) : 'no survey struct: a page built without patches/shader_survey.c';
})()
