//Custom lib
using dlg_kadens;
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Hashing;
using System.IO.Pipes;
using System.IO.Pipes;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;


namespace TestUIProject
{
    public partial class Form1 : Form
    {
        //constants
        const int PIPE_BUFFER_SIZE = 1024;
        const string VOXEL_CONVERTER_DEFAULT_FILEPATH = "voxelConversion.exe";
        const string VOXEL_PIPE_NAME = "VoxelPipe";
        //Networking
        string espAddr = "192.168.4.1";
        int espPort = 3333;
        Socket socket;
        //Processes/Pipes
        byte[] pipeBuffer = new byte[PIPE_BUFFER_SIZE];
        NamedPipeServerStream voxelPipe;
        Process voxelConverterProcess;
        string voxelConverterProcessFilepath = VOXEL_CONVERTER_DEFAULT_FILEPATH;


        public Form1()
        {
            InitializeComponent();
            UI_Textbox_Output.TextChanged += UI_Textbox_Output_TextChangedHandler;
            checkForConverterPath();//Check if the convert is present, if not continuously prompt for the file
        }

        ///////////////////////////////////////////////////////////////////
        //UI Event handlers
        ///////////////////////////////////////////////////////////////////
        private void UI_Textbox_Output_TextChangedHandler(object? sender, EventArgs e) {//perform auto scrolling
            UI_Textbox_Output.SelectionStart = UI_Textbox_Output.Text.Length;
            UI_Textbox_Output.ScrollToCaret();
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
            string objFilepath;
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "Obj files (*.obj) | *.obj";
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                objFilepath = openFileDialog.FileName;
                if (!File.Exists(objFilepath)) {//this should never trigger because of the fileDialog but we will check anyways. Throw a messagebox since this is a really rare edge issue.
                    MessageBox.Show($"File {objFilepath} not found."); ;
                    return;
                }
                //Run the converter to get data from an OBJ
                callVoxelConverterToConvert(objFilepath);
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
                UI_Button_Send_Tea.Enabled = true;
                UI_Button_Disconnect.Enabled = true;
                //clientRx();
            }
        }

        private void UI_Button_Send_Test_Click(object sender, EventArgs e)
        {
            clientTxRando();
        }

        private void UI_Button_Disconnect_Click(object sender, EventArgs e) {
            HandleDisconnect();
        }
        ///////////////////////////////////////////////////////////////////
        //Process Utils
        ///////////////////////////////////////////////////////////////////
        private async Task callVoxelConverterToConvert(string objFilepath) {
            if (voxelConverterProcess != null && !voxelConverterProcess.HasExited) { //Check if the process is running. Kill the process if its still running
                voxelConverterProcess.Kill();
                voxelConverterProcess.WaitForExit();
            }
            //start the pipe server
            UI_Textbox_Output.Text += "Creating pipe...\r\n";
            voxelPipe = new NamedPipeServerStream(VOXEL_PIPE_NAME, PipeDirection.In);

            //setup the converter process call
            UI_Textbox_Output.Text += "Calling converter process...\r\n";
            voxelConverterProcess = new Process();
            voxelConverterProcess.StartInfo.FileName = voxelConverterProcessFilepath;
            voxelConverterProcess.StartInfo.ArgumentList.Clear();
            voxelConverterProcess.StartInfo.ArgumentList.Add(objFilepath);
            voxelConverterProcess.StartInfo.ArgumentList.Add("-cp");
            voxelConverterProcess.StartInfo.UseShellExecute = false;
            voxelConverterProcess.OutputDataReceived += Process_OutputDataReceived;
            voxelConverterProcess.EnableRaisingEvents = true;
            voxelConverterProcess.StartInfo.RedirectStandardOutput = true;
            voxelConverterProcess.StartInfo.RedirectStandardError = true;
            voxelConverterProcess.StartInfo.UseShellExecute = false;
            voxelConverterProcess.StartInfo.CreateNoWindow = true;
            voxelConverterProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            voxelConverterProcess.Start();
            voxelConverterProcess.BeginOutputReadLine();
            //get the data from the pipe
            await voxelPipe.WaitForConnectionAsync();//wait for the pipe to be populated but dont hang the UI thread
            UI_Textbox_Output.Text += "UI got pipe connection from converter!\r\n";
            byte[] voxelBytes = await readAllDataInFromPipe(voxelPipe);
            UI_Textbox_Output.Text += $"Data from pipe:";
            foreach (int val in voxelBytes) {
                UI_Textbox_Output.Text += $" {val},";
            }
            //cleanup process
            await voxelConverterProcess.WaitForExitAsync();//wait for the process to exit AFTER we have flushed STDOUT to the UI
            voxelConverterProcess.WaitForExit();
        }

        private void checkForConverterPath() {
            string converterPath = voxelConverterProcessFilepath;
            while(!File.Exists(converterPath)) {
                MessageBox.Show($"WARNING: Converter .exe file not found. Please select the voxelConverter");
                OpenFileDialog openFileDialog = new OpenFileDialog();
                openFileDialog.Filter = "executable files (*.exe) | *.exe";
                if (openFileDialog.ShowDialog() == DialogResult.OK) {
                    converterPath = openFileDialog.FileName;
                }
                else {//close the program if the user doesn't want to provide an executable
                    MessageBox.Show($"WARNING: Hologram server requires the converter executable to run, closing program...");
                    Application.Exit();
                    Environment.Exit(0);
                    return;
                }
            }
            voxelConverterProcessFilepath = converterPath;
        }

        ///////////////////////////////////////////////////////////////////
        //Process Async/Events
        ///////////////////////////////////////////////////////////////////
        private async Task<byte[]> readAllDataInFromPipe(NamedPipeServerStream pipe) {
            using MemoryStream ms = new MemoryStream();
            byte[] buffer = new byte[PIPE_BUFFER_SIZE];
            while (true) {
                int bytesRead = await pipe.ReadAsync(buffer, 0, buffer.Length);
                if (bytesRead == 0) break;//pipe closed, exit the 
                ms.Write(buffer, 0, bytesRead);
            }
            return ms.ToArray();
        }

        private void Process_OutputDataReceived(object sender, DataReceivedEventArgs e) {
            if (e.Data != null) {
                UI_Textbox_Output.BeginInvoke(new Action(() => {
                    UI_Textbox_Output.AppendText($"{e.Data}\r\n");
                }));
            }
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

        private async void clientTxTea()

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
                msgHead.SliceCount = 16;
                msgHead.PayloadBytes = msgHead.FrameCount * msgHead.SliceCount * 64;
                msgHead.MotorSpeedRpm = (short)WifiTxMotor.Off;

                byte[] payload = demoFrames.teapot;
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

        private async void clientTxRando()
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
                msgHead.DataType = (sbyte)WifiTxDataType.Animation3D;
                msgHead.FrameCount = 20;
                msgHead.SliceCount = 8;
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
            UI_Button_Send_Tea.Enabled = false;
            UI_Button_Disconnect.Enabled = false;
            UI_Textbox_Output.Text += "\r\nNot Connected";
        }

        private void UI_Button_Send_Tea_Click(object sender, EventArgs e)
        {
            clientTxTea();
        }
    }
}
