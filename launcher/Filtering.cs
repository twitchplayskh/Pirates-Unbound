using System;
using System.Drawing;
using System.Windows.Forms;
class FilteringForm:Form {
 readonly ComboBox mode=new ComboBox(),af=new ComboBox();
 public int Filter {get{return mode.SelectedIndex;}}
 public int Anisotropy {get{return new[]{0,2,4,8,16}[af.SelectedIndex];}}
 public FilteringForm(int filter,int anisotropy){
  Text="Texture filtering";Font=new Font("Segoe UI",10);AutoScaleMode=AutoScaleMode.None;ClientSize=new Size(600,340);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterParent;
  BackColor=Color.FromArgb(24,34,43);ForeColor=Color.FromArgb(237,232,218);
  Controls.Add(new Label{Text="Texture filtering",Bounds=new Rectangle(18,23,205,26)});
  mode.SetBounds(225,18,355,32);mode.DropDownStyle=ComboBoxStyle.DropDownList;mode.Items.AddRange(new object[]{"Native game settings","Bilinear","Trilinear (recommended)"});mode.SelectedIndex=filter;Controls.Add(mode);
  Controls.Add(new Label{Text="Anisotropic filtering",Bounds=new Rectangle(18,71,205,26)});
  af.SetBounds(225,66,355,32);af.DropDownStyle=ComboBoxStyle.DropDownList;af.Items.AddRange(new object[]{"Native / no override","2×","4×","8×","16× (recommended)"});af.SelectedIndex=Array.IndexOf(new[]{0,2,4,8,16},anisotropy);Controls.Add(af);
  Controls.Add(new Label{Text="Sharpens world textures viewed at an angle. Higher levels use more GPU processing. The GPU's supported limit applies automatically. UI and captured backgrounds keep their native filtering. Applies on the next launch.",Bounds=new Rectangle(18,110,562,160)});
  var apply=new Button{Text="Use settings",DialogResult=DialogResult.OK,Bounds=new Rectangle(300,294,130,30)};
  var cancel=new Button{Text="Cancel",DialogResult=DialogResult.Cancel,Bounds=new Rectangle(452,294,128,30)};Controls.Add(apply);Controls.Add(cancel);AcceptButton=apply;CancelButton=cancel;
  using(var graphics=Graphics.FromHwnd(IntPtr.Zero)){float scale=graphics.DpiX/96f;if(scale>1.01f){foreach(Control control in Controls)control.Font=new Font(control.Font.FontFamily,control.Font.Size*scale,control.Font.Style);Scale(new SizeF(scale,scale));}}
 }
}
