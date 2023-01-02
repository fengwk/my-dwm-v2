/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 5;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;     /* 0 means no systray */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 0;        /* 0 means bottom bar */
static const unsigned int gappih    = 10;       /* horiz inner gap between windows */
static const unsigned int gappiv    = 10;       /* vert inner gap between windows */
static const unsigned int gappoh    = 10;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = 10;       /* vert outer gap between windows and screen edge */
static const int movewinthresholdv  = 8; /* 垂直：这个阈值越大movewin操作改变的范围越小 */
static const int movewinthresholdh  = 8; /* 水平：这个阈值越大movewin操作改变的范围越小 */
static const int resizewinthresholdv= 10; /* 垂直：这个阈值越大resizewin操作改变的范围越小 */
static const int resizewinthresholdh= 20; /* 水平：这个阈值越大resizewin操作改变的范围越小 */
static const char *fonts[]          = { "SauceCodePro Nerd Font Mono:pixelsize=32" };
static const char dmenufont[]       = "SauceCodePro Nerd Font Mono:pixelsize=32";
// 淡雅灰配色
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#444444";
// static const char col_sboard[]        = "#7799AA";
static const char col_sboard[]        = "#bbbbbb";
// 全黑配色
// static const char col_gray1[]       = "#000000";
// static const char col_gray2[]       = "#000000";
// static const char col_gray3[]       = "#bbbbbb";
// static const char col_gray4[]       = "#bbbbbb";
// static const char col_cyan[]        = "#222222";
// static const char col_sboard[]        = "#7799AA";
static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const char *colors[][3]      = {
  /*               fg         bg         border   */
  [SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
  [SchemeSel]  = { col_gray4, col_cyan,  col_sboard },
};
static const unsigned int alphas[][3]      = {
  /*               fg      bg        border     */
  [SchemeNorm] = { OPAQUE, baralpha, borderalpha },
  [SchemeSel]  = { OPAQUE, baralpha, borderalpha },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

// 展示标签主客户端标题
static const char ptagf[] = "%s %s";  /* format of a tag label */
static const char etagf[] = "%s";  /* format of an empty tag */
static const TagMapEntry tagnamemap[] = {
  { "st-256color", "" },
  { "Alacritty", "" },
  { "Google-chrome", "" },
  { "jetbrains-idea", "" },
  { "jetbrains-idea-ce", "" },
  { "code-oss", "" },
  { "com-xk72-charles-gui-MainWithClassLoader", "" },
  { "popo", "﫢" },
  { "wechat.exe", "" },
  { "Postman", "" },
  { "XMind", "" },
  { "Java", "" },
  { "Eclipse", "" },
  { "xiaoyi_assistant", "嬨" },
  { "vlc", "嗢" },
  { "baidunetdisk", "" },
  { "et", "" },
  { "wps", "" },
  { "wpp", "" },
  { "obs", "辶" },
  { "Optimus Manager Qt", "" },
  { "Nm-connection-editor", "" },
  { "Xfce4-power-manager-settings", "" },
  { "fcitx5-config-qt", "" },
  { "flameshot", "" },
  { "Peek", "" },
  { "Parcellite", "" },
  { "thunderbird", "" },
  { "Typora", "" },
  { "Timeshift-gtk", "" },
};

// https://dwm.suckless.org/customisation/rules/
// 使用xprop命令可以获取窗口信息
static const Rule rules[] = {
  /* xprop(1):
   *	WM_CLASS(STRING) = instance, class
   *	WM_NAME(STRING) = title
   */
  /* class            instance    title    tags mask    isfloating    monitor    ignoretransient */
  { "Peek",           NULL,       NULL,    0,           1,            -1,        0               },
  { "popo",           NULL,       NULL,    0,           1,            -1,        0               },
  { "wechat.exe",     NULL,       NULL,    0,           1,            -1,        0               },
  { "feh",            NULL,       NULL,    0,           1,            -1,        0               },
  { "jetbrains-idea", NULL,       NULL,    0,           0,            -1,        1               },
  { "jetbrains-idea-ce", NULL,    NULL,    0,           0,            -1,        1               },
  { "com-xk72-charles-gui-MainWithClassLoader", NULL, "Find in Session 1", 0, 1, -1, 0 },
  { "netease-cloud-music", NULL, NULL, 0, 1, -1, 0 },
};

// overview
static const char *overviewtag = "OVERVIEW";

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
  /* symbol     arrange function    append */
  { "[]=",      tile,               0 },    /* first entry is default */
  { "[M]",      monocle,            0 },    /* 单个窗口铺满屏幕 */
  { "###",      grid,               1 },    /* 垂直网格布局 */
  { "><>",      NULL,               0 },    /* no layout function means floating behavior */
};

/* key definitions */
#define NOMODKEY 0
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
  { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
  { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
  { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
  { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *rofi_win[] = { "rofi", "-show", "window", NULL };
static const char *rofi_run[] = { "rofi", "-show", "run", NULL };
static const char *rofi_drun[] = { "rofi", "-show", "drun", NULL };
// static const char *termcmd[]  = { "env", "LANG=en_US.UTF-8", "LANGUAGE=en_US", "st", NULL };
static const char *termcmd[]  = { "env", "LANG=en_US.UTF-8", "LANGUAGE=en_US", "alacritty", NULL };
static const char scratchpadname[] = "scratchpad";
static const char *scratchpadcmd[] = { "env", "LANG=en_US.UTF-8", "LANGUAGE=en_US", "alacritty", "-t", scratchpadname, NULL };
static const char *flameshotcmd[] = { "env", "QT_AUTO_SCREEN_SCALE_FACTOR=0", "QT_SCREEN_SCALE_FACTORS=''", "flameshot", "gui", NULL };
static const char *flameshotocrcmd[] = { "flameshotocr.sh", NULL };
// static const char *scratchpadcmd[] = { "env", "LANG=en_US.UTF-8", "LANGUAGE=en_US", "st", "-t", scratchpadname, "-g", "120x34", NULL };
static const char *monitordetection[] = { "monitor-detection.sh", NULL };

/* 
 * xev命令可以获取keycode
 * xmodmap命令可以查看所有modkey
 *
 * Mod1Mask 是alt键
 * Mod4Mask 是win键
 */
static const Key keys[] = {
  /* modifier                     key           function         argument */
  // { MODKEY,                       XK_p,         spawn,           {.v = dmenucmd } },
  { MODKEY,                       XK_w,         spawn,           {.v = rofi_win } },
  { MODKEY,                       XK_p,         spawn,           {.v = rofi_run } },
  { MODKEY|ShiftMask,             XK_p,         spawn,           {.v = rofi_drun } },
  { MODKEY|ShiftMask,             XK_Return,    spawn,           {.v = termcmd } },
  { MODKEY|ShiftMask,             XK_KP_Enter,  spawn,           {.v = termcmd } },
  { MODKEY,                       XK_grave,     togglescratch,   {.v = scratchpadcmd } }, // 打开临时命令行窗口
  { MODKEY,                       XK_Print,     spawn,           {.v = flameshotocrcmd } }, // 截图ocr
  { NOMODKEY,                     XK_Print,     spawn,           {.v = flameshotcmd } }, // 截图
  { MODKEY,                       XK_Escape,    spawn,           {.v = monitordetection } }, // 屏幕检测
  { MODKEY,                       XK_b,         togglebar,       {0} }, // 状态栏开关
  { MODKEY,                       XK_j,         focusstack,      {.i = +1 } }, // 向栈底移动
  { MODKEY,                       XK_k,         focusstack,      {.i = -1 } }, // 向栈顶移动
  { MODKEY,                       XK_i,         incnmaster,      {.i = +1 } }, // 增加主工作区数量
  { MODKEY,                       XK_d,         incnmaster,      {.i = -1 } }, // 减少主工作区数量
  { MODKEY,                       XK_h,         setmfact,        {.f = -0.05} }, // 减少主工作区空间
  { MODKEY,                       XK_l,         setmfact,        {.f = +0.05} }, // 增加主工作区空间
  { MODKEY|Mod4Mask|ShiftMask,    XK_BackSpace, togglesmartgaps, {0} }, // 智能间隙开关（仅有一个client时是否显示间隙）
  { MODKEY|Mod4Mask,              XK_BackSpace, togglegaps,      {0} }, // 间隙开关
  { MODKEY|Mod4Mask,              XK_0,         defaultgaps,     {0} }, // 重置间隙
  { MODKEY|Mod4Mask,              XK_equal,     incrgaps,        {.i = +1 } }, // 增大间隙
  { MODKEY|Mod4Mask,              XK_minus,     incrgaps,        {.i = -1 } }, // 减少间隙
  // { MODKEY|Mod4Mask,              XK_h,         incrgaps,        {.i = +1 } },
  // { MODKEY|Mod4Mask,              XK_l,         incrgaps,        {.i = -1 } },
  // { MODKEY|Mod4Mask|ShiftMask,    XK_h,         incrogaps,       {.i = +1 } },
  // { MODKEY|Mod4Mask|ShiftMask,    XK_l,         incrogaps,       {.i = -1 } },
  // { MODKEY|Mod4Mask|ControlMask,  XK_h,         incrigaps,       {.i = +1 } },
  // { MODKEY|Mod4Mask|ControlMask,  XK_l,         incrigaps,       {.i = -1 } },
  // { MODKEY,                       XK_y,         incrihgaps,      {.i = +1 } }, // 增大垂直内侧间隙
  // { MODKEY,                       XK_o,         incrihgaps,      {.i = -1 } }, // 增大垂直内侧间隙
  // { MODKEY|ControlMask,           XK_y,         incrivgaps,      {.i = +1 } }, // 增大水平内侧间隙
  // { MODKEY|ControlMask,           XK_o,         incrivgaps,      {.i = -1 } }, // 缩小水平内侧间隙
  // { MODKEY|Mod4Mask,              XK_y,         incrohgaps,      {.i = +1 } }, // 增大垂直外侧间隙
  // { MODKEY|Mod4Mask,              XK_o,         incrohgaps,      {.i = -1 } }, // 缩小垂直外侧间隙
  // { MODKEY|ShiftMask,             XK_y,         incrovgaps,      {.i = +1 } }, // 增大水平外侧间隙
  // { MODKEY|ShiftMask,             XK_o,         incrovgaps,      {.i = -1 } }, // 缩小水平外侧间隙
  { MODKEY,                       XK_Return,    zoom,            {0} },
  { MODKEY,                       XK_KP_Enter,  zoom,            {0} },
  { Mod4Mask,                     XK_f,         togglefloating,  {0} },              // 窗口浮动开关
  { Mod4Mask,                     XK_Up,        movewin,         {.ui = UP} },       // 向上移动窗口
  { Mod4Mask,                     XK_Down,      movewin,         {.ui = DOWN} },     // 向下移动窗口
  { Mod4Mask,                     XK_Left,      movewin,         {.ui = LEFT} },     // 向左移动窗口
  { Mod4Mask,                     XK_Right,     movewin,         {.ui = RIGHT} },    // 向右移动窗口
  { Mod4Mask,                     XK_k,         movewin,         {.ui = UP} },       // 向上移动窗口
  { Mod4Mask,                     XK_j,         movewin,         {.ui = DOWN} },     // 向下移动窗口
  { Mod4Mask,                     XK_h,         movewin,         {.ui = LEFT} },     // 向左移动窗口
  { Mod4Mask,                     XK_l,         movewin,         {.ui = RIGHT} },    // 向右移动窗口
  { MODKEY|Mod4Mask,              XK_Up,        resizewin,       {.ui = V_REDUCE} }, // 垂直减少窗口大小
  { MODKEY|Mod4Mask,              XK_Down,      resizewin,       {.ui = V_EXPAND} }, // 垂直增加窗口大小
  { MODKEY|Mod4Mask,              XK_Left,      resizewin,       {.ui = H_REDUCE} }, // 水平减少窗口大小
  { MODKEY|Mod4Mask,              XK_Right,     resizewin,       {.ui = H_EXPAND} }, // 水平增加窗口大小
  { MODKEY|Mod4Mask,              XK_k,         resizewin,       {.ui = V_REDUCE} }, // 垂直减少窗口大小
  { MODKEY|Mod4Mask,              XK_j,         resizewin,       {.ui = V_EXPAND} }, // 垂直增加窗口大小
  { MODKEY|Mod4Mask,              XK_h,         resizewin,       {.ui = H_REDUCE} }, // 水平减少窗口大小
  { MODKEY|Mod4Mask,              XK_l,         resizewin,       {.ui = H_EXPAND} }, // 水平增加窗口大小
  { MODKEY,                       XK_Tab,       view,            {0} },
  { Mod4Mask,                     XK_w,         toggleoverview,  {0} },
  { MODKEY|ShiftMask,             XK_c,         killclient,      {0} },
  { MODKEY,                       XK_t,         setlayout,       {.v = &layouts[0]} },
  { MODKEY,                       XK_f,         setlayout,       {.v = &layouts[3]} },
  { MODKEY,                       XK_m,         setlayout,       {.v = &layouts[1]} },
  { MODKEY,                       XK_g,         setlayout,       {.v = &layouts[2]} },
  { MODKEY,                       XK_space,     setlayout,       {0} },
  { MODKEY,                       XK_0,         view,            {.ui = ~0 } },
  { MODKEY|ShiftMask,             XK_0,         tag,             {.ui = ~0 } },
  { MODKEY,                       XK_comma,     focusmon,        {.i = -1 } },
  { MODKEY,                       XK_period,    focusmon,        {.i = +1 } },
  { MODKEY|ShiftMask,             XK_comma,     tagmon,          {.i = -1 } },
  { MODKEY|ShiftMask,             XK_period,    tagmon,          {.i = +1 } },
  { MODKEY|ShiftMask,             XK_Left,      viewtoleft,      {0} },
  { MODKEY|ShiftMask,             XK_Right,     viewtoright,     {0} },
  { MODKEY|ShiftMask,             XK_h,         viewtoleft,      {0} },
  { MODKEY|ShiftMask,             XK_l,         viewtoright,     {0} },
  { MODKEY,                       XK_bracketleft,  viewtoleft,   {0} },
  { MODKEY,                       XK_bracketright, viewtoright,  {0} },
  TAGKEYS(                        XK_1,                          0)
  TAGKEYS(                        XK_2,                          1)
  TAGKEYS(                        XK_3,                          2)
  TAGKEYS(                        XK_4,                          3)
  TAGKEYS(                        XK_5,                          4)
  TAGKEYS(                        XK_6,                          5)
  TAGKEYS(                        XK_7,                          6)
  TAGKEYS(                        XK_8,                          7)
  TAGKEYS(                        XK_9,                          8)
  { MODKEY|ShiftMask,             XK_q,         quit,            {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
  /* click                event mask      button          function        argument */
  { ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
  { ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
  { ClkWinTitle,          0,              Button2,        zoom,           {0} },
  { ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
  { ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
  { ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
  { ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
  { ClkTagBar,            0,              Button1,        view,           {0} },
  { ClkTagBar,            0,              Button3,        toggleview,     {0} },
  { ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
  { ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
  { ClkTagBar,            0,              Button4,        viewtoleft,     {0} },       // 在tag栏上鼠标上滚切换到上一个tag
  { ClkTagBar,            0,              Button5,        viewtoright,    {0} },       // 在tag栏上鼠标上滚切换到下一个tag
  { ClkWinTitle,          0,              Button4,        focusstack,     {.i = -1} }, // 在标题栏上鼠标上滚切换到上一个客户端
  { ClkWinTitle,          0,              Button5,        focusstack,     {.i = +1} }, // 在标题栏上鼠标上滚切换到下一个客户端
};
