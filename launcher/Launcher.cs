using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;

public class Preferences {
 public int Width=2560, Height=1440;
 public bool Fullscreen=true, Patch=true, World=true, CenterUI=true, FillBackgrounds=true;
 public bool ExperimentalSailing120=false;
 public int MSAA=4;
}
public class Recovery { public string Config, Backup; }
static class Storage {
 public static readonly string Root=AppDomain.CurrentDomain.BaseDirectory;
 public static readonly string Config=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"My Games", "Sid Meier's Pirates!", "Config.ini");
 public static string Prefs {get{return Path.Combine(Root,"settings.xml");}}
 public static string Pending {get{return Path.Combine(Root,"backups","pending-config.xml");}}
 public static T Read<T>(string path) {
  using(var reader=XmlReader.Create(path,new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit}))
   return (T)new XmlSerializer(typeof(T)).Deserialize(reader);
 }
 public static void Write<T>(string path,T value) {
  Directory.CreateDirectory(Path.GetDirectoryName(path));
  var temp=path+".new";
  using(var file=File.Create(temp))new XmlSerializer(typeof(T)).Serialize(file,value);
  if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
 }
 public static bool IsRunning(){return Process.GetProcessesByName("Pirates!").Any();}
 public static bool Recover(){
  if(!File.Exists(Pending))return false;
  if(IsRunning())throw new InvalidOperationException("Close Pirates before restoring the display settings.");
  var state=Read<Recovery>(Pending);
  var backupRoot=Path.GetFullPath(Path.Combine(Root,"backups"))+Path.DirectorySeparatorChar;
  if(!String.Equals(Path.GetFullPath(state.Config),Path.GetFullPath(Config),StringComparison.OrdinalIgnoreCase)||!Path.GetFullPath(state.Backup).StartsWith(backupRoot,StringComparison.OrdinalIgnoreCase))
   throw new InvalidOperationException("The display recovery record has an unexpected path.");
  File.Copy(state.Backup,Config,true);File.Delete(Pending);return true;
 }
 public static string Backup(){
  if(File.Exists(Pending))throw new InvalidOperationException("Restore the previous display settings first.");
  var folder=Path.Combine(Root,"backups",DateTime.Now.ToString("yyyyMMdd-HHmmss-fff"));Directory.CreateDirectory(folder);
  var backup=Path.Combine(folder,"Config.ini");File.Copy(Config,backup);
  foreach(var save in Directory.GetFiles(Path.GetDirectoryName(Config),"*.pirates_savegame"))File.Copy(save,Path.Combine(folder,Path.GetFileName(save)));
  Write(Pending,new Recovery{Config=Config,Backup=backup});return backup;
 }
 public static string SetValue(string text,string key,int value){
  var pattern=@"(?m)^[ \t]*"+Regex.Escape(key)+@"[ \t]*=[^\r\n]*";
  var setting=key+" = "+value;
  return Regex.IsMatch(text,pattern)?Regex.Replace(text,pattern,setting):text+"\r\n"+setting+"\r\n";
 }
 public static string Quote(string value){
  var output=new StringBuilder("\"");int slashes=0;
  foreach(char c in value){
   if(c=='\\'){slashes++;continue;}
   if(c=='"'){output.Append('\\',slashes*2+1);output.Append(c);}
   else{output.Append('\\',slashes);output.Append(c);}slashes=0;
  }
  output.Append('\\',slashes*2);output.Append('"');return output.ToString();
 }
 public static string Hash(string path){using(var sha=SHA256.Create())using(var file=File.OpenRead(path))return BitConverter.ToString(sha.ComputeHash(file)).Replace("-","");}
 public static void Validate(Preferences p){if(p.Width<640||p.Width>3840||p.Height<480||p.Height>2160||3*p.Width<4*p.Height)throw new InvalidOperationException("Choose a resolution from 640×480 to 3840×2160, with a 4:3 or wider aspect ratio.");if(p.MSAA!=0&&p.MSAA!=2&&p.MSAA!=4&&p.MSAA!=8)throw new InvalidOperationException("Choose Off, 2×, 4× or 8× anti-aliasing.");}
}
static class Session {
 public static string RuntimeSettings(Preferences p){
  return "[Widescreen]\r\nWorld="+(p.World?1:0)+"\r\nCenterUI="+(p.CenterUI?1:0)+"\r\nFillBackgrounds="+(p.FillBackgrounds?1:0)+"\r\nBorderless="+(p.Fullscreen?1:0)+"\r\nExperimentalSailing120="+(p.Patch&&p.ExperimentalSailing120?1:0)+"\r\nMSAA="+(p.Patch?p.MSAA:0)+"\r\n";
 }
 // Fullscreen preferences now mean a normal D3D window covering its monitor.
 // Keeping FullScreen=0 avoids exclusive-mode device loss on focus changes.
 [DllImport("user32.dll",SetLastError=true)]static extern int GetWindowLong(IntPtr window,int index);
 [DllImport("user32.dll",SetLastError=true)]static extern int SetWindowLong(IntPtr window,int index,int value);
 [DllImport("user32.dll",SetLastError=true)]static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
 static async Task Borderless(Process game){
  var deadline=DateTime.UtcNow.AddSeconds(30);IntPtr window=IntPtr.Zero;
  while(DateTime.UtcNow<deadline&&!game.HasExited){game.Refresh();window=game.MainWindowHandle;if(window!=IntPtr.Zero)break;await Task.Delay(100);}
  if(window==IntPtr.Zero)throw new InvalidOperationException("The Pirates window did not appear for borderless fullscreen.");
  var bounds=Screen.FromHandle(window).Bounds;
  const int styleIndex=-16,extendedIndex=-20;
  SetWindowLong(window,styleIndex,(GetWindowLong(window,styleIndex)&~0x00cf0000)|unchecked((int)0x80000000));
  SetWindowLong(window,extendedIndex,GetWindowLong(window,extendedIndex)&~0x00000301);
  if(!SetWindowPos(window,IntPtr.Zero,bounds.X,bounds.Y,bounds.Width,bounds.Height,0x0034))
   throw new InvalidOperationException("Could not size the borderless Pirates window: "+Marshal.GetLastWin32Error());
 }
 public static async Task<string> Tool(string exe,string args){
  using(var process=new Process{StartInfo=new ProcessStartInfo(exe,args){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true}}){
   process.Start();var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();
   await Task.Run(()=>process.WaitForExit());var text=(await output)+(await error);
   if(process.ExitCode!=0)throw new InvalidOperationException(text.Trim().Length>0?text.Trim():"A launcher helper failed.");return text;
  }
 }
 public static async Task Play(Preferences p,string exe,string[] args,Action<string> status,Action hide){
  Storage.Validate(p);
  if(Storage.IsRunning())throw new InvalidOperationException("Pirates is already running. Close it before starting another session.");
  if(!File.Exists(exe)||Path.GetFileName(exe)!="Pirates!.exe"||Storage.Hash(exe)!="5342209C16EA847FA6AC9B90F25A20B46012AEF24D1B5F8A7C7FACDFE174E2E7")
   throw new InvalidOperationException("This prototype supports the inspected Steam 1.0.2.0 Pirates executable. Its file check did not match.");
  var runtime=Path.Combine(Storage.Root,"runtime");var dll=Path.Combine(runtime,"PiratesWide.dll");
  if(p.Patch&&(!File.Exists(dll)||!File.Exists(Path.Combine(runtime,"inject.exe"))))throw new InvalidOperationException("Widescreen runtime files are missing.");
  Storage.Write(Storage.Prefs,p);Storage.Backup();Process game=null;
  try{
   var text=File.ReadAllText(Storage.Config,Encoding.Default);
   text=Storage.SetValue(text,"WindowWidth",p.Width);text=Storage.SetValue(text,"WindowHeight",p.Height);text=Storage.SetValue(text,"FullScreen",0);
   File.WriteAllText(Storage.Config,text,Encoding.Default);
   File.WriteAllText(Path.Combine(runtime,"PiratesWide.ini"),RuntimeSettings(p));
   var log=Path.Combine(runtime,"PiratesWide.log");if(p.Patch)File.WriteAllText(log,"");
   status("Starting Pirates…");
   var start=new ProcessStartInfo(exe,String.Join(" ",args.Select(Storage.Quote))){UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(exe)};
   start.EnvironmentVariables["SteamAppId"]="3920";start.EnvironmentVariables["SteamGameId"]="3920";
   game=Process.Start(start);hide();
   if(p.Patch){
    await Tool(Path.Combine(runtime,"inject.exe"),game.Id+" "+Storage.Quote(dll));
    var deadline=DateTime.UtcNow.AddSeconds(65);bool ready=false;
    while(DateTime.UtcNow<deadline){
     if(game.HasExited)throw new InvalidOperationException("Pirates exited before the widescreen patch initialized.");
     string output="";try{using(var reader=new StreamReader(new FileStream(log,FileMode.Open,FileAccess.Read,FileShare.ReadWrite)))output=reader.ReadToEnd();}catch(IOException){}
     if(output.Contains("init=OK")){ready=true;break;}
     if(output.Contains("mismatch")||output.Contains("unavailable")||output.Contains("Unexpected")||output.Contains("init=FAILED"))throw new InvalidOperationException("Widescreen initialization failed. See runtime/PiratesWide.log.");
     await Task.Delay(200);
    }
    if(!ready)throw new InvalidOperationException("Widescreen startup timed out. Bring the game to the foreground during startup.");
   }
   if(p.Fullscreen){
    if(!p.Patch){game.Refresh();var monitor=Screen.FromHandle(game.MainWindowHandle).Bounds;
     if(p.Width!=monitor.Width||p.Height!=monitor.Height)throw new InvalidOperationException("Borderless fullscreen at a scaled resolution requires the patch for mouse alignment. Enable the patch or select the monitor resolution.");}
    await Borderless(game);
   }
   await Task.Run(()=>game.WaitForExit());
  }catch{
   if(game!=null&&!game.HasExited){game.Kill();game.WaitForExit(5000);}throw;
  }finally{
   if(game!=null)game.Dispose();Storage.Recover();
  }
 }
}
class LauncherForm:Form {
 readonly NumericUpDown width=new NumericUpDown(),height=new NumericUpDown();
 readonly ComboBox presets=new ComboBox();
 readonly ComboBox antialiasing=new ComboBox();
 readonly CheckBox fullscreen=new CheckBox(),patch=new CheckBox(),world=new CheckBox(),ui=new CheckBox(),backgrounds=new CheckBox(),sailing=new CheckBox();
 readonly Label status=new Label();
 readonly Button play=new Button(),save=new Button(),restore=new Button();
 readonly string exe;readonly string[] gameArgs;bool busy,syncing;
 readonly bool layoutOnly=File.Exists(Path.Combine(Storage.Root,"runtime","layout-only.build"));
 public LauncherForm(string executable,string[] args){
  exe=executable;gameArgs=args;Text="Pirates! — Widescreen Launcher";
  AutoScaleMode=AutoScaleMode.None;
  Font=new Font("Segoe UI",10);ClientSize=new Size(550,665);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;
  BackColor=Color.FromArgb(24,34,43);ForeColor=Color.FromArgb(237,232,218);
  var title=AddLabel("Sid Meier’s Pirates!",24,18,500,42,new Font("Segoe UI",19,FontStyle.Bold));title.AutoSize=true;
  AddLabel("Widescreen settings",26,65,500,25,null);
  AddLabel("Resolution",26,103,110,25,null);presets.SetBounds(150,100,370,30);presets.DropDownStyle=ComboBoxStyle.DropDownList;
  presets.Items.AddRange(new object[]{"1280 × 720","1920 × 1080","2560 × 1440","3840 × 2160","Custom"});Controls.Add(presets);
  width.Minimum=640;width.Maximum=3840; height.Minimum=480;height.Maximum=2160;
  width.SetBounds(150,142,120,30);height.SetBounds(307,142,120,30);Controls.Add(width);Controls.Add(height);AddLabel("×",283,144,24,25,null);
  SetCheck(fullscreen,"Borderless fullscreen",150,185);
  SetCheck(patch,"Enable widescreen patch",26,231);
  SetCheck(world,"Expand the world view horizontally",45,267);
  SetCheck(ui,"Keep the UI in 4:3 and align mouse clicks",45,303);
  SetCheck(backgrounds,"Fill the screen with menu and overlay backgrounds",45,339);
  AddLabel("Anti-aliasing",45,384,140,26,null);antialiasing.SetBounds(195,380,325,30);antialiasing.DropDownStyle=ComboBoxStyle.DropDownList;
  antialiasing.Items.AddRange(new object[]{"Off","2× MSAA","4× MSAA (recommended)","8× MSAA"});Controls.Add(antialiasing);
  SetCheck(sailing,"Experimental: sailing at 120 FPS",45,434);
  AddLabel(layoutOnly?"High-FPS research is disabled in this release.":"Unfinished: movement uses the original update rate.",45,465,475,28,new Font("Segoe UI",9));
  var controls=new Button{Text="Controls / hotkeys",Bounds=new Rectangle(26,504,220,32)};Controls.Add(controls);
  controls.Click+=(s,e)=>{try{using(var menu=new ControlsForm())menu.ShowDialog(this);}catch(Exception error){ShowError(error);}};
  status.SetBounds(26,550,500,42);status.ForeColor=Color.FromArgb(183,197,209);Controls.Add(status);
  restore.Text="Restore display";restore.SetBounds(26,619,139,32);save.Text="Save settings";save.SetBounds(185,619,139,32);play.Text="Play";play.SetBounds(365,617,155,36);
  foreach(var button in new[]{restore,save,play,controls}){button.FlatStyle=FlatStyle.Flat;button.BackColor=Color.FromArgb(40,60,73);button.ForeColor=ForeColor;Controls.Add(button);}
  play.BackColor=Color.FromArgb(163,117,39);AcceptButton=play;
  var tip=new ToolTip();tip.SetToolTip(fullscreen,"Covers the monitor without exclusive fullscreen. Resolution selects the game's rendering size; the image fills the monitor. Leave unchecked for a normal window.");tip.SetToolTip(world,"Preserves vertical field of view and exposes additional world at the sides.");tip.SetToolTip(ui,"Includes the matching mouse-coordinate correction and UI clipping.");tip.SetToolTip(backgrounds,"Keeps captured world backgrounds aligned behind dialogs. Some painted backgrounds stretch.");
  tip.SetToolTip(sailing,"Targets 120 FPS only while sailing on the world map. Gameplay keeps its original update cadence. Experimental: interpolation, effects, and transitions are still being tested. Requires the patch; change applies on the next launch.");
  tip.SetToolTip(antialiasing,"Smooths polygon edges without a blur filter. Higher levels use more GPU memory and processing. Falls back to a supported lower level, or Off. Applies on the next launch.");
  using(var graphics=Graphics.FromHwnd(IntPtr.Zero)){
   float scale=graphics.DpiX/96f;
   if(scale>1.01f){foreach(Control control in Controls)control.Font=new Font(control.Font.FontFamily,control.Font.Size*scale,control.Font.Style);Scale(new SizeF(scale,scale));}
  }
  Preferences p=new Preferences();try{if(File.Exists(Storage.Prefs))p=Storage.Read<Preferences>(Storage.Prefs);Storage.Validate(p);}catch(Exception e){status.Text="Settings reset: "+e.Message;p=new Preferences();}
  width.Value=p.Width;height.Value=p.Height;fullscreen.Checked=p.Fullscreen;patch.Checked=p.Patch;world.Checked=p.World;ui.Checked=p.CenterUI;backgrounds.Checked=p.FillBackgrounds;sailing.Checked=p.ExperimentalSailing120;antialiasing.SelectedIndex=p.MSAA==0?0:p.MSAA==2?1:p.MSAA==4?2:3;SyncPreset();EnableOptions();
  presets.SelectedIndexChanged+=(s,e)=>{if(!syncing&&presets.SelectedIndex<4){syncing=true;var sizes=new[]{new[]{1280,720},new[]{1920,1080},new[]{2560,1440},new[]{3840,2160}};width.Value=sizes[presets.SelectedIndex][0];height.Value=sizes[presets.SelectedIndex][1];syncing=false;}};
  width.ValueChanged+=(s,e)=>{if(!syncing)SyncPreset();};height.ValueChanged+=(s,e)=>{if(!syncing)SyncPreset();};
  patch.CheckedChanged+=(s,e)=>EnableOptions();ui.CheckedChanged+=(s,e)=>EnableOptions();
  save.Click+=(s,e)=>{try{var prefs=GetPreferences();Storage.Validate(prefs);Storage.Write(Storage.Prefs,prefs);status.Text="Settings saved for the next Steam launch.";}catch(Exception error){ShowError(error);}};
  restore.Click+=(s,e)=>{try{status.Text=Storage.Recover()?"Original display settings restored.":"Your original display settings are already restored.";}catch(Exception error){ShowError(error);}};
  play.Click+=async(s,e)=>{
   if(busy)return;busy=true;SetBusy(true);
   try{await Session.Play(GetPreferences(),exe,gameArgs,message=>status.Text=message,()=>Hide());busy=false;Close();}
   catch(Exception error){busy=false;Show();Activate();SetBusy(false);ShowError(error);}
  };
  FormClosing+=(s,e)=>{if(busy)e.Cancel=true;};
  Shown+=(s,e)=>{try{if(Storage.Recover())status.Text="Recovered your original display settings from the previous session.";else if(String.IsNullOrEmpty(status.Text))status.Text="Original display settings restore when you quit.";}catch(Exception error){ShowError(error);}};
 }
 Label AddLabel(string text,int x,int y,int w,int h,Font font){var label=new Label{Text=text};label.SetBounds(x,y,w,h);if(font!=null)label.Font=font;Controls.Add(label);return label;}
 void SetCheck(CheckBox control,string text,int x,int y){control.Text=text;control.SetBounds(x,y,490-x,30);control.AutoSize=true;Controls.Add(control);}
 void SyncPreset(){syncing=true;string value=width.Value+" × "+height.Value;presets.SelectedIndex=presets.Items.IndexOf(value);if(presets.SelectedIndex<0)presets.SelectedIndex=4;syncing=false;}
 void EnableOptions(){world.Enabled=ui.Enabled=antialiasing.Enabled=patch.Checked&&!busy;sailing.Enabled=patch.Checked&&!busy&&!layoutOnly;if(layoutOnly)sailing.Checked=false;backgrounds.Enabled=patch.Checked&&ui.Checked&&!busy;}
 void SetBusy(bool value){foreach(Control c in Controls)c.Enabled=!value;status.Enabled=true;EnableOptions();}
 Preferences GetPreferences(){return new Preferences{Width=(int)width.Value,Height=(int)height.Value,Fullscreen=fullscreen.Checked,Patch=patch.Checked,World=world.Checked,CenterUI=ui.Checked,FillBackgrounds=backgrounds.Checked,ExperimentalSailing120=sailing.Checked,MSAA=new[]{0,2,4,8}[antialiasing.SelectedIndex]};}
 void ShowError(Exception error){status.Text=error.Message;MessageBox.Show(this,error.Message,"Pirates! launcher",MessageBoxButtons.OK,MessageBoxIcon.Warning);}
}
static class Program {
 [DllImport("user32.dll")]static extern bool SetProcessDPIAware();
 [DllImport("shell32.dll",CharSet=CharSet.Unicode)]static extern IntPtr CommandLineToArgvW(string command,out int count);
 [DllImport("kernel32.dll")]static extern IntPtr LocalFree(IntPtr memory);
 [STAThread]static int Main(string[] args){
  if(args.Length>0&&args[0]=="--self-test")return SelfTest();
  bool created;using(var mutex=new Mutex(true,"Local\\PiratesWideLauncher3920",out created)){
   if(!created)return 0;
   SetProcessDPIAware();Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
   var exe=args.Length>0?args[0]:Path.Combine(Path.GetDirectoryName(Storage.Root.TrimEnd(Path.DirectorySeparatorChar)),"Pirates!.exe");
   Application.Run(new LauncherForm(Path.GetFullPath(exe),args.Skip(1).ToArray()));return 0;
  }
 }
 static int SelfTest(){
  try{
   foreach(var input in new[]{"hello","path with spaces","abc\\","a\\\"b","","E:\\path with spaces\\","a\"b"}){
    int count;var arguments=CommandLineToArgvW("launcher.exe "+Storage.Quote(input),out count);
    if(arguments==IntPtr.Zero)throw new Exception("Argument parser failed");
    try{if(count!=2||Marshal.PtrToStringUni(Marshal.ReadIntPtr(arguments,IntPtr.Size))!=input)throw new Exception("Argument round trip failed");}finally{LocalFree(arguments);}
   }
   var original="[User Settings]\r\nWindowWidth = 1920\r\nWindowHeight = 1440\r\nMyName = Incognito\r\n";
   var updated=Storage.SetValue(original,"WindowWidth",3840);
   if(updated!="[User Settings]\r\nWindowWidth = 3840\r\nWindowHeight = 1440\r\nMyName = Incognito\r\n")throw new Exception("Config edit changed unrelated settings");
   Storage.Validate(new Preferences{Width=3840,Height=2160});
   var legacy=new XmlSerializer(typeof(Preferences));
   using(var reader=new StringReader("<Preferences><Width>3840</Width><Height>2160</Height></Preferences>"))
    if(((Preferences)legacy.Deserialize(reader)).ExperimentalSailing120)throw new Exception("Old settings unexpectedly enable experimental sailing");
   var experimental=new Preferences{ExperimentalSailing120=true};
   foreach(int level in new[]{0,2,4,8}){
    var aa=new Preferences{MSAA=level};Storage.Validate(aa);
    if(!Session.RuntimeSettings(aa).Contains("MSAA="+level+"\r\n"))throw new Exception("MSAA preference not passed to runtime");
    aa.Patch=false;if(!Session.RuntimeSettings(aa).Contains("MSAA=0\r\n"))throw new Exception("MSAA enabled without patch");
   }
   bool aaRejected=false;try{Storage.Validate(new Preferences{MSAA=3});}catch(InvalidOperationException){aaRejected=true;}if(!aaRejected)throw new Exception("Invalid MSAA level accepted");
   if(!Session.RuntimeSettings(experimental).Contains("ExperimentalSailing120=1\r\n"))throw new Exception("Sailing preference not passed to runtime");
   experimental.Patch=false;
   if(!Session.RuntimeSettings(experimental).Contains("ExperimentalSailing120=0\r\n"))throw new Exception("Sailing enabled without patch");
   bool rejected=false;try{Storage.Validate(new Preferences{Width=640,Height=2160});}catch(InvalidOperationException){rejected=true;}if(!rejected)throw new Exception("Invalid aspect accepted");
   var keymap="; Keep this comment\r\n[Sail]\r\nFullSails_Num8 = Num8 ; sails\r\nReefedSails_Num2 = Num2\r\nTurnLeft_Num4 = Num4\r\nTurnRight_Num6 = Num6\r\nAttackShip_a = a\r\nQuickSave_S = S\r\nQuickLoad_L = L\r\n[Fight]\r\nTarget_Tab = Tab ; targets\r\n[Sneak]\r\nRun_Shift = Shift\r\n[Unknown]\r\nOther=unmodified\r\n";
   var keys=GameControls.Parse(keymap);if(keys.Count!=9)throw new Exception("Keymap parser failed");
   GameControls.Wasd(keys,"Sail");var edited=GameControls.Rewrite(keymap,keys);
   if(!edited.StartsWith("; Keep this comment\r\n")||!edited.EndsWith("[Unknown]\r\nOther=unmodified\r\n")||!edited.Contains("FullSails_Num8 = w ; sails\r\n")||!edited.Contains("QuickSave_S = Key372\r\n")||!edited.Contains("QuickLoad_L = Key376\r\n")||!edited.Contains("Target_Key9 = Key9 ; targets\r\n")||!edited.Contains("Run_Key16 = Key272\r\n"))throw new Exception("Keymap rewrite/preset/native token conversion failed");
   if(GameControls.Rewrite(edited,GameControls.Parse(edited))!=edited)throw new Exception("Keymap round trip failed");
   var lf="[Sail]\nTurnLeft_Num4=a ; comment\nTurnRight_Num6=d";
   if(GameControls.Rewrite(lf,GameControls.Parse(lf))!="[Sail]\nTurnLeft_Num4=a ; comment\nTurnRight_Num6=d")throw new Exception("Keymap newline preservation failed");
   keys[1].Token="W";rejected=false;try{GameControls.Rewrite(keymap,keys);}catch(InvalidOperationException){rejected=true;}if(!rejected)throw new Exception("Duplicate controls accepted");
   keys[0].Token="Num8";keys[1].Token="Key294";rejected=false;try{GameControls.Rewrite(keymap,keys);}catch(InvalidOperationException){rejected=true;}if(!rejected)throw new Exception("Native key aliases accepted as separate controls");
   keys[1].Token="UnknownKey";rejected=false;try{GameControls.Rewrite(keymap,keys);}catch(InvalidOperationException){rejected=true;}if(!rejected)throw new Exception("Invalid control accepted");
   File.WriteAllText(Path.Combine(Storage.Root,"launcher-tests.log"),"PASS: config edits, validation, argument quoting, legacy settings, sailing controls, keymap preservation, WASD presets, native key tokens and conflict rejection.\r\n");return 0;
  }catch(Exception e){File.WriteAllText(Path.Combine(Storage.Root,"launcher-tests.log"),e.ToString());return 1;}
 }
}

