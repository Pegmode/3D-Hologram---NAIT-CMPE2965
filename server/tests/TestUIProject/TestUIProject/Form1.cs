using System.Diagnostics;
using System.Threading;


namespace TestUIProject
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void UI_Button_RUN_Click(object sender, EventArgs e)
        {
            Process process = new Process();
            process.StartInfo.FileName = "voxelConversion.exe";
            process.StartInfo.UseShellExecute = false;
            process.OutputDataReceived += Process_OutputDataReceived;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.EnableRaisingEvents = true;
            process.Start();
            process.WaitForExit();
            string t = process.StandardOutput.ReadToEnd();
            UI_Textbox_OUtput.AppendText(t);

        }

        private void Process_OutputDataReceived(object sender, DataReceivedEventArgs e)
        {
            UI_Textbox_OUtput.Text += "stuff";
            if (e.Data != null)
            {
                UI_Textbox_OUtput.AppendText(e.Data);
            }

        }

        private void UI_Button_Visualize_Click(object sender, EventArgs e)
        {
            Process process = new Process();
            process.StartInfo.FileName = "voxelConversion.exe";
            process.StartInfo.Arguments = "-dv";
            process.StartInfo.UseShellExecute = false;
            process.OutputDataReceived += Process_OutputDataReceived;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.EnableRaisingEvents = true;
            process.Start();
            process.WaitForExit();
            string t = process.StandardOutput.ReadToEnd();
            UI_Textbox_OUtput.AppendText(t);
        }

        private void UI_Button_LoadObj_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "Obj files (*.obj) | *.obj";
            if (openFileDialog.ShowDialog() == DialogResult.OK) {
                string filePath = openFileDialog.FileName;
                
            }


        }
    }
}
