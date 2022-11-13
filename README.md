# My Dwm

个人dwm环境。

# Installation

1. 克隆并安装

    ```shell
    git clone git@github.com:fengwk/my-dwm.git ~/prog/my-dwm
    cd ~/prog/my-dwm
    sudo make clean install
    ```

1. 安装渲染器

    ```shell
    sudo pacman -S picom
    ```

1. 配置`~/.xinitrc`

   ```shell
   # 多屏幕时可能需要用到下边的指令
   # xrandr --output eDP1 --scale 0.5x0.5

   # Xresources
   [[ -f ~/.Xresources ]] && xrdb -merge -I$HOME ~/.Xresources

   # Xmodmap
   [[ -f ~/.Xmodmap ]] && xmodmap ~/.Xmodmap

   # xbinkeys
   killall xbindkeys
   xbindkeys

   # fix java application
   export _JAVA_AWT_WM_NONREPARENTING=1
   export AWT_TOOLKIT=MToolkit
   wmname LG3D

   exec dwm

   # fix nvidia
   # 使用nvidia+startx启动会失败以至于黑屏，使用下边的命令可以进入但不能解决nvidia启用失败，使用
   sddm启动不会有这个问题
   # xrandr --auto
   ```

1. 配置`~/.Xresources`

   ```shell
   # 4k屏的HiDPI配置
   Xft.dpi: 192
   Xft.autohint: 0
   Xft.lcdfilter:  lcddefault
   Xft.hintstyle:  hintfull
   Xft.hinting: 1
   Xft.antialias: 1
   Xft.rgba: rgb
   ```

1. 配置`~/.Xmodmap`

   ```shell
   # MSI P15
   # keycode  93 = XF86TouchpadToggle NoSymbol XF86TouchpadToggle
   ```

1. 在`~/.local/share/dwm`目录里配置dwm启动脚本，见[my-dwm-autostart](https://github.com/fengwk/my-dwm-autostart)

1. 安装[nerd-fonts](https://github.com/ryanoasis/nerd-fonts)

# Usage

- `alt + t` - 窗口模式：平铺模式，[]=
- `alt + f` - 窗口模式：浮动模式，><>
- `alt + m` - 窗口模式：monocle，单窗口独占屏幕（有bar），[M]
- `alt + space` - 在当前窗口模式与前一个窗口模式间切换
- `alt + F` - 全屏开关（无bar）
- `alt + shift + enter` - 打开终端
- `alt + p` - 打开dmenu
- `alt + j` - 向窗口栈下一层移动
- `alt + k` - 向窗口栈上一层移动
- `alt + C` - 关闭当前窗口
- `alt + Q` - 关闭dwm
- `alt + [1-9]` - 切换到相应标签页
- `alt + shift + [1-9]` - 将当前窗口移动到相应标签页
- `alt + ctrl + [1-9]` - 将多个标签页合并
- `alt + 0` - 将所有标签页合并
- `alt + ,` - 移动到左侧显示器屏幕（可循环）
- `alt + .` - 移动到右侧显示器屏幕（可循环）
- `alt + <` - 将当前窗口移动到右侧显示器屏幕
- `alt + >` - 将当前窗口移动到右侧显示器屏幕
- `alt + i` or `alt + d` - 改变窗口布局，比如第一次按`ctl+d`可以变为水平分屏，按`ctl+i`可以转回垂直分屏
- `alt + 鼠标右键拖拽` - 调整窗口大小
- `alt + 鼠标左键拖拽` - 移动窗口位置
- `alt + 鼠标中键` - 回到平铺
