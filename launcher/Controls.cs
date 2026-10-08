using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

sealed class GameBinding {
 public int Line;
 public string Mode,Name,Source,Token,Default;
 public string Action {get{return Regex.Replace(Name.Substring(0,Name.LastIndexOf('_')),"([a-z])([A-Z])","$1 $2");}}
}
static class GameControls {
 public static readonly string PathName=Path.Combine(Path.GetDirectoryName(Storage.Config),"KeyMap.ini");
 public static readonly string Original=Path.Combine(Storage.Root,"backups","controls","original-keymap.ini");
 public static readonly string[] Modes={"Sail","Battle","Dance","Duel","Fight","Sneak"};
 public static readonly Dictionary<string,string> Keys=CreateKeys();
 static Dictionary<string,string> CreateKeys(){
  var keys=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
  for(char c='A';c<='Z';c++)keys.Add(c.ToString(),Char.ToLowerInvariant(c).ToString());
  for(char c='0';c<='9';c++)keys.Add(c.ToString(),"Key"+(int)c);
  for(int i=1;i<=9;i++)keys.Add("Numpad "+i,"Num"+i);
  for(int i=1;i<=12;i++)keys.Add("F"+i,"Key"+(367+i));
  keys.Add("Space","Space");keys.Add("Tab","Key9");keys.Add("Shift","Key272");keys.Add("Ctrl","Key273");keys.Add("Alt","Key274");
  keys.Add("Enter","Key13");keys.Add("Escape","Key27");keys.Add("Backspace","Key8");
  keys.Add("Left arrow","Key293");keys.Add("Up arrow","Key294");keys.Add("Right arrow","Key295");keys.Add("Down arrow","Key296");
  keys.Add("Home","Key292");keys.Add("End","Key291");keys.Add("Page Up","Key289");keys.Add("Page Down","Key290");
  keys.Add("Insert","Key301");keys.Add("Delete","Key302");
  return keys;
 }
 public static string Canonical(string token){
  if(token.Equals("Tab",StringComparison.OrdinalIgnoreCase))return "Key9";
  if(token.Equals("Shift",StringComparison.OrdinalIgnoreCase))return "Key272";
  if(token.Equals("Ctrl",StringComparison.OrdinalIgnoreCase))return "Key273";
  if(token.Equals("Key16",StringComparison.OrdinalIgnoreCase))return "Key272";
  if(token.Equals("Key17",StringComparison.OrdinalIgnoreCase))return "Key273";
  return Keys.Values.FirstOrDefault(v=>v.Equals(token,StringComparison.OrdinalIgnoreCase));
 }
 public static string Label(string token){var canonical=Canonical(token);return Keys.FirstOrDefault(k=>k.Value.Equals(canonical,StringComparison.OrdinalIgnoreCase)).Key??token;}
 static string ConflictKey(string token){
  string canonical=Canonical(token)??token;
  int[] native={0,291,296,290,293,268,295,292,294,289};
  if(canonical.StartsWith("Num",StringComparison.Ordinal)&&canonical.Length==4&&canonical[3]>='1'&&canonical[3]<='9')return "Key"+native[canonical[3]-'0'];
  return canonical;
 }
 public static List<GameBinding> Parse(string text){
  var rows=new List<GameBinding>();string mode="";var lines=Regex.Split(text,"\r\n|\n|\r");
  for(int i=0;i<lines.Length;i++){
   var section=Regex.Match(lines[i],@"^\s*\[([^\]]+)\]");if(section.Success){mode=section.Groups[1].Value;continue;}
   var match=Regex.Match(lines[i],@"^\s*([^;=\s]+)\s*=\s*([^;\s]+)");
   if(!Modes.Contains(mode)||!match.Success)continue;
   string name=match.Groups[1].Value;int split=name.LastIndexOf('_');if(split<1)continue;
   string source=name.Substring(split+1),token=match.Groups[2].Value;
   rows.Add(new GameBinding{Line=i,Mode=mode,Name=name,Source=source,Token=token,Default=source});
  }
  return rows;
 }
 public static string Rewrite(string original,List<GameBinding> rows){
  var conflicts=rows.GroupBy(r=>r.Mode+"|"+ConflictKey(r.Token),StringComparer.OrdinalIgnoreCase).FirstOrDefault(g=>g.Count()>1);
  if(conflicts!=null)throw new InvalidOperationException("Choose different keys for "+String.Join(" and ",conflicts.Select(r=>r.Action))+" in "+conflicts.First().Mode+".");
  // Parse uses logical line numbers; preserve each line's newline and comments.
  var logical=Regex.Matches(original,@"[^\r\n]*(?:\r\n|\n|\r|$)").Cast<Match>().Where(m=>m.Length>0).Select(m=>m.Value).ToArray();
  foreach(var row in rows){
   var token=Canonical(row.Token);if(token==null)throw new InvalidOperationException("Unsupported key for "+row.Action+": "+row.Token);
   string source=Canonical(row.Source);if(source==null)throw new InvalidOperationException("Unknown original key for "+row.Action+".");
   // The native parser accepts letters, Space, NumN and numeric KeyNN.
   // Tab/Shift are shipped as names it ignores; normalize both sides.
   string name=row.Name.Substring(0,row.Name.LastIndexOf('_')+1)+(row.Source.Equals("Tab",StringComparison.OrdinalIgnoreCase)||row.Source.Equals("Shift",StringComparison.OrdinalIgnoreCase)||row.Source.Equals("Ctrl",StringComparison.OrdinalIgnoreCase)?source:row.Source);
   // Held-action identifiers use raw VK codes; incoming keydown events add 256.
   if(row.Source.Equals("Shift",StringComparison.OrdinalIgnoreCase))name=row.Name.Substring(0,row.Name.LastIndexOf('_')+1)+"Key16";
   if(row.Source.Equals("Ctrl",StringComparison.OrdinalIgnoreCase))name=row.Name.Substring(0,row.Name.LastIndexOf('_')+1)+"Key17";
   logical[row.Line]=Regex.Replace(logical[row.Line],@"^(\s*)[^;=\s]+(\s*=\s*)[^;\s]+",m=>m.Groups[1].Value+name+m.Groups[2].Value+token);
  }
  return String.Concat(logical);
 }
 public static string Save(string original,List<GameBinding> rows){
  if(Storage.IsRunning())throw new InvalidOperationException("Quit Pirates before saving controls. New bindings apply on the next launch.");
  string updated=Rewrite(original,rows);
  if(File.ReadAllText(PathName,Encoding.Default)!=original)throw new InvalidOperationException("KeyMap.ini changed since this menu opened. Close and reopen Controls before saving.");
  var folder=Path.Combine(Storage.Root,"backups","controls");Directory.CreateDirectory(folder);
  if(!File.Exists(Original))File.Copy(PathName,Original);
  var backup=Path.Combine(folder,DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".ini");
  // Backups may live on a different drive; File.Replace's backup must not.
  File.Copy(PathName,backup);
  // The game reads byte strings; Unicode INI files silently lose mappings.
  // Keep an exact byte backup, then emit its native Windows code page.
  Encoding encoding=Encoding.GetEncoding(1252,EncoderFallback.ExceptionFallback,DecoderFallback.ExceptionFallback);
  string temp=PathName+".pirateswide-new";File.WriteAllText(temp,updated,encoding);
  try{File.Replace(temp,PathName,null);}finally{if(File.Exists(temp))File.Delete(temp);}
  return updated;
 }
 public static void Wasd(List<GameBinding> rows,string mode){
  if(mode=="Duel")throw new InvalidOperationException("Set duel actions individually; they are attacks and defenses rather than movement directions.");
  foreach(var row in rows.Where(r=>r.Mode==mode)){
   string action=row.Name.Substring(0,row.Name.LastIndexOf('_'));string key=null;
   switch(action){
    case "FullSails":case "Forward":key="W";break;
    case "ReefedSails":case "Back":key="S";break;
    case "TurnLeft":case "Left":key="A";break;
    case "TurnRight":case "Right":key="D";break;
    case "UpLeft":case "CircleLeft":key="Q";break;
    case "UpRight":case "CircleRight":key="E";break;
    case "DownLeft":key="Z";break;case "DownRight":key="C";break;
    case "AttackShip":key="Space";break;
    case "QuickSave":key="F5";break;case "QuickLoad":key="F9";break;
   }
   if(key!=null)row.Token=Keys[key];
  }
 }
}
sealed class ControlsForm:Form {
 readonly TabControl tabs=new TabControl();
 readonly Dictionary<string,DataGridView> grids=new Dictionary<string,DataGridView>();
 readonly Label status=new Label();
 List<GameBinding> rows;string original;bool dirty;
 public ControlsForm(){
  AutoScaleMode=AutoScaleMode.None;Text="Pirates! — Controls";Font=new Font("Segoe UI",10);StartPosition=FormStartPosition.CenterParent;MinimumSize=new Size(746,659);ClientSize=new Size(730,620);
  original=File.ReadAllText(GameControls.PathName,Encoding.Default);rows=GameControls.Parse(original);
  if(rows.Count==0)throw new InvalidOperationException("No supported bindings were found in KeyMap.ini.");
  var intro=new Label{Text="Choose keys for each game mode. Changes apply when you next start Pirates.",AutoSize=false,Bounds=new Rectangle(18,14,690,42),Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right};Controls.Add(intro);
  tabs.SetBounds(18,62,694,420);tabs.Anchor=AnchorStyles.Top|AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(tabs);
  string[] names={"Sailing","Ship battle","Dancing","Dueling","Land battle","Sneaking"};
  for(int i=0;i<GameControls.Modes.Length;i++){
   string mode=GameControls.Modes[i];var page=new TabPage(names[i]){Tag=mode};tabs.TabPages.Add(page);
   var grid=new DataGridView{Dock=DockStyle.Fill,AllowUserToAddRows=false,AllowUserToDeleteRows=false,AllowUserToResizeRows=false,RowHeadersVisible=false,AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill,BackgroundColor=Color.White,SelectionMode=DataGridViewSelectionMode.FullRowSelect,MultiSelect=false,EditMode=DataGridViewEditMode.EditOnEnter};
   grid.ColumnHeadersHeightSizeMode=DataGridViewColumnHeadersHeightSizeMode.AutoSize;grid.RowTemplate.Height=28;
   grid.Columns.Add(new DataGridViewTextBoxColumn{Name="Action",HeaderText="Action",ReadOnly=true,FillWeight=140});
   grid.Columns.Add(new DataGridViewComboBoxColumn{Name="Key",HeaderText="Your key",DataSource=GameControls.Keys.Keys.ToList(),FillWeight=100});
   grid.Columns.Add(new DataGridViewTextBoxColumn{Name="Default",HeaderText="Original key",ReadOnly=true,FillWeight=85});
   foreach(var row in rows.Where(r=>r.Mode==mode)){int n=grid.Rows.Add(row.Action,GameControls.Label(row.Token),GameControls.Label(row.Default));grid.Rows[n].Tag=row;}
   grid.CurrentCellDirtyStateChanged+=(s,e)=>{if(grid.IsCurrentCellDirty)grid.CommitEdit(DataGridViewDataErrorContexts.Commit);};
   grid.CellValueChanged+=(s,e)=>{if(e.RowIndex>=0&&e.ColumnIndex==1){var binding=(GameBinding)grid.Rows[e.RowIndex].Tag;binding.Token=GameControls.Keys[(string)grid.Rows[e.RowIndex].Cells[1].Value];dirty=true;}};
   grid.DataError+=(s,e)=>{status.Text="This entry uses a key outside the editor's supported list.";e.ThrowException=false;};
   page.Controls.Add(grid);grids.Add(mode,grid);
  }
  var note=new Label{Text="Keyboard keys only. Built-in numpad shortcuts and on-screen key hints may remain unchanged. Escape and other menu keys still have their usual functions.",Bounds=new Rectangle(18,489,694,44),Anchor=AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right};Controls.Add(note);
  var preset=ButtonAt("WASD for this mode",18,538,165);var reset=ButtonAt("Reset this mode",193,538,140);var restore=ButtonAt("Restore backup",343,538,140);
  var save=ButtonAt("Save controls",492,577,135);var cancel=ButtonAt("Close",638,577,74);
  status.SetBounds(18,578,455,30);status.Anchor=AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(status);
  preset.Click+=(s,e)=>{try{string mode=(string)tabs.SelectedTab.Tag;GameControls.Wasd(rows,mode);RefreshMode(mode);dirty=true;status.Text="Preset selected. Click Save controls to apply.";}catch(Exception error){Error(error);}};
  reset.Click+=(s,e)=>{string mode=(string)tabs.SelectedTab.Tag;foreach(var row in rows.Where(r=>r.Mode==mode))row.Token=GameControls.Canonical(row.Default);RefreshMode(mode);dirty=true;status.Text="Original keys selected. Click Save controls to apply.";};
  save.Click+=(s,e)=>{try{foreach(var grid in grids.Values)grid.EndEdit();original=GameControls.Save(original,rows);rows=GameControls.Parse(original);RebindRows();dirty=false;restore.Enabled=true;status.Text="Saved. Start Pirates to use these keys.";}catch(Exception error){Error(error);}};
  restore.Enabled=File.Exists(GameControls.Original);
  restore.Click+=(s,e)=>{try{if(Storage.IsRunning())throw new InvalidOperationException("Quit Pirates before restoring controls.");var backupRows=GameControls.Parse(File.ReadAllText(GameControls.Original,Encoding.Default));foreach(var row in rows){var backup=backupRows.FirstOrDefault(r=>r.Mode==row.Mode&&r.Action==row.Action);if(backup!=null)row.Token=backup.Token;}foreach(var mode in grids.Keys)RefreshMode(mode);dirty=true;status.Text="Backup keys selected. Click Save controls to apply.";}catch(Exception error){Error(error);}};
  cancel.Click+=(s,e)=>Close();FormClosing+=(s,e)=>{if(dirty&&MessageBox.Show(this,"Close without saving your control changes?","Unsaved controls",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)e.Cancel=true;};
  using(var graphics=Graphics.FromHwnd(IntPtr.Zero)){
   float scale=graphics.DpiX/96f;
   if(scale>1.01f){
    Scale(new SizeF(scale,scale));
    foreach(var grid in grids.Values)foreach(DataGridViewRow row in grid.Rows)row.Height=(int)Math.Ceiling(28*scale);
   }
  }
 }
 Button ButtonAt(string text,int x,int y,int w){var b=new Button{Text=text,Bounds=new Rectangle(x,y,w,32),Anchor=AnchorStyles.Bottom|AnchorStyles.Left};Controls.Add(b);return b;}
 void RefreshMode(string mode){foreach(DataGridViewRow r in grids[mode].Rows)r.Cells[1].Value=GameControls.Label(((GameBinding)r.Tag).Token);}
 void RebindRows(){foreach(var mode in grids.Keys){var bindingRows=rows.Where(r=>r.Mode==mode).ToArray();for(int i=0;i<bindingRows.Length;i++)grids[mode].Rows[i].Tag=bindingRows[i];}}
 void Error(Exception e){status.Text=e.Message;MessageBox.Show(this,e.Message,"Pirates! controls",MessageBoxButtons.OK,MessageBoxIcon.Warning);}
}
