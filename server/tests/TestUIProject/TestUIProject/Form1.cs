using System.Diagnostics;
using System.Net.Sockets;
using System.Text.Json;
using System.Text;
using System.Threading;
using System.Xml.Linq;
using System.Security.Cryptography;
using System.IO.Hashing;
using System.IO;
//Custom lib
using dlg_kadens;
using System.IO.Pipes;

namespace TestUIProject
{
    public partial class Form1 : Form
    {
        //constants
        const int PIPE_BUFFER_SIZE = 1024;
        const string VOXEL_CONVERTER_DEFAULT_FILEPATH = "voxelConversion.exe";
        //Networking
        string espAddr = "192.168.4.1";
        int espPort = 3333;
        Socket socket;
        //Processes/Pipes
        byte[] pipeBuffer = new byte[PIPE_BUFFER_SIZE];


        public Form1()
        {
            InitializeComponent();
        }

        ///////////////////////////////////////////////////////////////////
        //UI Event handlers
        ///////////////////////////////////////////////////////////////////
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
            UI_Textbox_Output.AppendText(t);

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
            UI_Textbox_Output.AppendText(t);
        }

        private void UI_Button_LoadObj_Click(object sender, EventArgs e)
        {
            string filePath;
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "Obj files (*.obj) | *.obj";
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                filePath = openFileDialog.FileName;
                if (!File.Exists(filePath)) {//this should never trigger because of the fileDialog but we will check anyways. Throw a messagebox since this is a really rare edge issue.
                    MessageBox.Show($"File {filePath} not found."); ;
                    return;
                }
                //open a pipe....
                UI_Textbox_Output.Text += "Creating pipe for conversion...";
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
                UI_Textbox_Output.Text += "\r\nConnected";
                UI_Button_Connect.Enabled = false;
                UI_Button_Send_Test.Enabled = true;
                UI_Button_Disconnect.Enabled = true;
                //clientRx();
            }
        }

        private void UI_Button_Send_Test_Click(object sender, EventArgs e)
        {
            clientTx();
        }

        private void UI_Button_Disconnect_Click(object sender, EventArgs e) {
            HandleDisconnect();
        }
        ///////////////////////////////////////////////////////////////////
        //Process Utils
        ///////////////////////////////////////////////////////////////////
        private void callVoxelConverter() { 
        
        
        }


        ///////////////////////////////////////////////////////////////////
        //Networking / Sockets
        ///////////////////////////////////////////////////////////////////

        private static async Task SendAllAsync(Socket socket, byte[] data, CancellationToken cancellationToken = default)
        {
            int totalSent = 0;

            while (totalSent < data.Length)
            {
                int sent = await socket.SendAsync(
                    data.AsMemory(totalSent, data.Length - totalSent),
                    SocketFlags.None,
                    cancellationToken);

                if (sent <= 0)
                {
                    throw new SocketException((int)SocketError.ConnectionReset);
                }

                totalSent += sent;
            }
        }

        private void Process_OutputDataReceived(object sender, DataReceivedEventArgs e) {
            UI_Textbox_Output.Text += "stuff";
            if (e.Data != null) {
                UI_Textbox_Output.AppendText(e.Data);
            }


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
                msgHead.Version = 2;
                msgHead.DataType = (sbyte)WifiTxDataType.Still3D;
                msgHead.FrameCount = 1;
                msgHead.SliceCount = 4;
                msgHead.PayloadBytes = msgHead.FrameCount * msgHead.SliceCount * 64;
                msgHead.MotorSpeedRpm = (short)WifiTxMotor.Off;

                byte[] payload = RandomNumberGenerator.GetBytes(msgHead.PayloadBytes);
                
                msgHead.PayloadCrc32 = Crc32.HashToUInt32(payload);

                byte[] header = msgHead.GetBytes();
                byte[] txBytes = new byte[header.Length + payload.Length];

                Buffer.BlockCopy(header, 0, txBytes, 0, header.Length);
                Buffer.BlockCopy(payload, 0, txBytes, header.Length, payload.Length);

                await SendAllAsync(socket, txBytes);

                Trace.WriteLine($"sent {txBytes.Length} bytes:\n{msgHead}");
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
            UI_Button_Send_Test.Enabled = false;
            UI_Button_Disconnect.Enabled = false;
            UI_Textbox_Output.Text += "\r\nNot Connected";
        }
    }
}
