using System.Diagnostics;
using System.Net.Sockets;
using System.Text.Json;
using System.Text;
using System.Threading;
using System.Xml.Linq;
using dlg_kadens;
using System.Security.Cryptography;


namespace TestUIProject
{
    public partial class Form1 : Form
    {
        string espAddr = "192.168.4.1";
        int espPort = 3333;
        Socket socket;

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
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                string filePath = openFileDialog.FileName;

            }


        }

        private void UI_Button_Connect_Click(object sender, EventArgs e)
        {
            //use Kaden's dialog dll for connection
            ConnDlg dlg = new(espAddr, espPort);
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                socket = dlg.ConnClient;
                espAddr = dlg.Address;
                espPort = dlg.Port;
                UI_Textbox_OUtput.Text += "Connected";
                UI_Button_Connect.Enabled = false;
                //clientRx();
            }
        }

        private void UI_Button_Send_Test_Click(object sender, EventArgs e)
        {
            clientTx();
        }

        private async void clientTx()
        {
            //check if connection is alive
            if (socket == null || !socket.Connected)
            {
                HandleDisconnect();
                return;
            }

            //serialize, encode and send message
            try
            {
                MessageHeader msgHead = new MessageHeader();
                msgHead.Version = 1;
                msgHead.DataType = 1;
                msgHead.FrameCount = 1;
                msgHead.SliceCount = 1;
                msgHead.PayloadBytes = 64;
                msgHead.MotorSpeedRpm = 456;
                msgHead.Flags = 123;
                msgHead.PayloadCrc32 = 789;
                

                List<byte> bytes = new List<byte>();
                bytes.AddRange(msgHead.GetBytes());
                bytes.AddRange(RandomNumberGenerator.GetBytes(64));

                byte[] txBytes = bytes.ToArray();
                int sent = await socket.SendAsync(txBytes, SocketFlags.None);
                Trace.WriteLine($"sent {sent} bytes:\n{msgHead.ToString()}");
            }
            catch (SocketException exc)
            {
                Trace.WriteLine("Send:SocketException : " + exc.Message);
                HandleDisconnect();
                return;
            }
            catch (Exception exc)
            {
                Trace.WriteLine("Send:Exception : " + exc.Message);
                HandleDisconnect();
                return;
            }
        }

        private void UI_Button_Disconnect_Click(object sender, EventArgs e)
        {
            HandleDisconnect();
        }

        //deal with loss of connection event
        private void HandleDisconnect()
        {
            try
            {
                if (socket != null && socket.Connected)
                {
                    socket.Shutdown(SocketShutdown.Both);
                }
                socket?.Close();
            }
            catch { }

            socket = null;
            UI_Button_Connect.Enabled = true;
            UI_Textbox_OUtput.Text += "Not Connected";
        }
    }
}
