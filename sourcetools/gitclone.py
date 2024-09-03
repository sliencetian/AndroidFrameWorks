import xml.etree.ElementTree as ET
import os
import subprocess
import shutil

# 获取当前文件的文件夹路径
current_folder_path = os.path.dirname(os.path.abspath(__file__))

DOWN_LOAD_CONTAINER='platform/build/'

nameFilter=DOWN_LOAD_CONTAINER

def run_git_command(command):
    print(f"run_git_command: {command}")
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate()
    # if process.returncode == 0:
    #     print("输出:", stdout)
    # else:
    #     print("错误:", stderr)
    return process.returncode == 0

def parse_default_xml(file_path):
    # 解析 XML 文件
    tree = ET.parse(file_path)
    root = tree.getroot()

    # 打印根元素
    print(f"Root Element: {root.tag}")

    # 遍历子元素
    for child in root:
        if child.tag == 'project':
            path=child.attrib['path']
            name=child.attrib['name']
            if 'platform/frameworks/base' in name:
                continue
            if 'platform/frameworks/av' in name:
                continue
            if 'platform/frameworks/compile/libbcc' in name:
                continue
            if 'platform/frameworks/compile/mclinker' in name:
                continue
            if 'platform/frameworks/compile/slang' in name:
                continue
            if 'platform/build/blueprint' in name:
                continue
            if 'platform/build/bazel' in name:
                continue
            if nameFilter in name:
                print(f"开始下载: {child.attrib}")
                # run_git_command(["git", "clone", "https://android.googlesource.com/platform/system/authgraph.git", f"{current_folder_path}/system/authgraph"])
                if run_git_command(["git", "clone", f"https://android.googlesource.com/{name}.git", f"{current_folder_path}/{path}"]):
                    print(f"     下载成功已检出: {child.attrib['name']}")
                else:
                    print("     ============下载失败")
                    # shutil.rmtree(f"{current_folder_path}/{path}")
                    # if run_git_command(["git", "clone", "--no-checkout", f"https://android.googlesource.com/{name}.git", f"{current_folder_path}/{path}"]):
                    #     print(f"     下载成功未检出: {child.attrib['name']}")
                    # else:
                    #     print("     下载失败")
                    exit()
            
        
        # 如果子元素还有子元素，进一步遍历
        # for subchild in child:
        #     print(f"  Subchild Element: {subchild.tag}, Text: {subchild.text}")

if __name__ == "__main__":
    # 替换为你的 default.xml 文件路径
    xml_file_path = f'{current_folder_path}/default.xml'
    parse_default_xml(xml_file_path)