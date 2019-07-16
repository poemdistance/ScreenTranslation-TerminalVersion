# 目前xdotool无法在本机(Arch + Gnome + terminator)终端中正常运行，程序暂时不完全可用 
# 待到来日再重写, 还有其他问题，等我一一解决...
# 不过在其他窗口中目前测试的都是可以正常运行的

# 程序功能
* 屏幕取词翻译

# 依赖
* C语言库: Xlib
* 终端命令行工具: xdotool, ps, awk, tail
* https://github.com/poemdistance/google-translate 项目的tranen程序, 安装事项等见项目内readme
 
# 编译
* $ gcc -g getClipboard.c  DetectMouse.c -o main -lX11 -lXtst -Wall

# 运行
* $ ./main

# 使用方法
* 用鼠标双击某个英文单词或者多次点击鼠标左键选中一整句、一整段，亦可按住鼠标左键选取想要翻译的文本, 程序会自动获取得到翻译结果在终端显示.

# 注意事项
1. 相关依赖如果没有请自行安装
2. 终端里要正常运行，请执行命令 

    $ ps -p \`xdotool getwindowfocus getwindowpid\` |awk '{print $NF}' | tail -n 1 

    将得到的终端应用名添加到DetectMouse.c的termName数组中并拓展数组容量，否则在监测终端复制文字的时候发送的是Ctrl-C，而不是真正的复制快捷键Ctrl-Shift-C.

3. **如果终端使用了Smart copy，在没有选中任何文字的时候，可能会使模拟发送的Ctrl-Shift-C被终端视为Ctrl-C而导致运行中的程序意外结束，这不是本程序的Bug，可以将Smart Copy关闭防止此类危险情况发生**

4. 本程序使用了本人两个代码库的项目，为屏幕取词(Extract-screen-word)和谷歌翻译(google-translate)，利用管道进行进程通信达到取词翻译目的

5. 本程序可能存在问题：管道通信速度可能不够（未觉察）

# 后期将完善
1. 现阶段代码只作为测试学习使用，后期将改用其他进程通信方式
2. 增加界面显示，而不是仅仅在终端里输出结果
