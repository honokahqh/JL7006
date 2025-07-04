import os
from pydub import AudioSegment

# 获取用户输入的音量增益（单位：dB）
try:
    gain_db = float(input("请输入要增加的音量（单位 dB，例如 3 或 -2）："))
except ValueError:
    print("输入错误，请输入数字")
    exit(1)

# 创建输出文件夹
output_dir = "output"
os.makedirs(output_dir, exist_ok=True)

# 遍历当前目录的所有 mp3 文件
for filename in os.listdir("."):
    if filename.lower().endswith(".mp3"):
        try:
            # 读取 mp3 文件
            audio = AudioSegment.from_mp3(filename)
            
            # 增加音量
            louder_audio = audio + gain_db
            
            # 输出到 output 文件夹
            output_path = os.path.join(output_dir, filename)
            louder_audio.export(output_path, format="mp3")
            print(f"处理完成：{filename} -> {output_path}")
        except Exception as e:
            print(f"处理文件 {filename} 时出错：{e}")

print("所有 MP3 文件处理完成。")
