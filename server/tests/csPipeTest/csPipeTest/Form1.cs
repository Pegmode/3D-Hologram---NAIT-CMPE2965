using System.IO.Pipes;

namespace csPipeTest
{
    public partial class Form1 : Form
    {

        NamedPipeServerStream pipeServer;
        byte[] buffer = new byte[1024];

        public Form1()
        {
            InitializeComponent();
        }

        private void UI_Button_CreatePipe_Click(object sender, EventArgs e)
        {
            byte[] sss = { 0, 1, 2, 3 };
            UI_Textbox_Output.Text += "Creating pipe ";
            pipeServer = new NamedPipeServerStream("VoxelPipe", PipeDirection.In);
            pipeServer.WaitForConnection();
            int bytesReadCount;
            while ((bytesReadCount = pipeServer.Read(buffer, 0, buffer.Length)) > 0) {
                UI_Textbox_Output.Text += "\n";
                foreach (byte val in buffer) {
                    UI_Textbox_Output.Text += $"{val.ToString()} ,";
                }
                
            }
        }
    }
}
