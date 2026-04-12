//Custom lib
using dlg_kadens;
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Hashing;
using System.IO.Pipes;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;


namespace TestUIProject {
    public partial class Form1 : Form
    {
        //constants
        const int PIPE_BUFFER_SIZE = 1024;
        const string VOXEL_CONVERTER_DEFAULT_FILEPATH = "voxelConversion.exe";
        const string VOXEL_PIPE_NAME = "VoxelPipe";
        readonly Dictionary<string, (Int32, Int32, byte[])> testBakedDataDict = new Dictionary<string, (Int32, Int32, byte[])>{
            { "", (0,0,[0]) },
            { "Teapot" , demoFrames.teapot },
            { "Arrow Wave", demoFrames.wave },
            { "Rando", demoFrames.Rando}
        };
        //Networking
        string espAddr = "192.168.4.1";
        int espPort = 3333;
        Socket socket;
        //Processes/Pipes
        byte[] storedTxMessage;//message for whatever is loaded 
        NamedPipeServerStream voxelPipe;
        Process voxelConverterProcess;
        string voxelConverterProcessFilepath = VOXEL_CONVERTER_DEFAULT_FILEPATH;


        public Form1()
        {
            InitializeComponent();
            UI_Textbox_Output.TextChanged += UI_Textbox_Output_TextChangedHandler;
            //List<string> testBakedDataTitles = ["", "Teapot"];

            UI_ComboBox_TestBakedData.DataSource = testBakedDataDict.Keys.ToList();
        }

        ///////////////////////////////////////////////////////////////////
        //UI Event handlers
        ///////////////////////////////////////////////////////////////////
        private void UI_Textbox_Output_TextChangedHandler(object? sender, EventArgs e)
        {//perform auto scrolling
            UI_Textbox_Output.SelectionStart = UI_Textbox_Output.Text.Length;
            UI_Textbox_Output.ScrollToCaret();
        }

        private void UI_Button_RUN_Click(object sender, EventArgs e)
        {
            //TODO

        }

        private void UI_Button_Visualize_Click(object sender, EventArgs e)
        {
            if (!converterExecutableExists())
            {//the the converter is not present, we can't run things that rely on it...
                return;
            }
            string objFilepath;
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "Obj files (*.obj) | *.obj";
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                objFilepath = openFileDialog.FileName;
                if (!File.Exists(objFilepath))
                {//this should never trigger because of the fileDialog but we will check anyways. Throw a messagebox since this is a really rare edge issue.
                    MessageBox.Show($"File {objFilepath} not found."); ;
                    return;
                }
                //Run the converter to get data from an OBJ
                callVoxelConverterToVisualize(objFilepath);
            }

        }

        private async void UI_Button_LoadObj_Click(object sender, EventArgs e)
        {
            if (!converterExecutableExists())
            {//the the converter is not present, we can't run things that rely on it...
                return;
            }
            string objFilepath;
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "Obj files (*.obj) | *.obj";
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                objFilepath = openFileDialog.FileName;
                if (!File.Exists(objFilepath))
                {//this should never trigger because of the fileDialog but we will check anyways. Throw a messagebox since this is a really rare edge issue.
                    MessageBox.Show($"File {objFilepath} not found."); ;
                    return;
                }
                //Run the converter to get data from an OBJ
                byte[] voxelBytes = await callVoxelConverterToConvert(objFilepath);
                Int32 currentFramecount = 1;//debug const
                Int32 currentSliceCount = 16;//standard...
                Int16 currentRPM = 100;//debug const
                storedTxMessage = MessageHeader.Build3DImageMessage(voxelBytes, currentFramecount, currentSliceCount, WifiTxDataType.Still3D, currentRPM);
                UI_Button_SendLoaded.Enabled = true;
                UI_Textbox_LoadedImageName.Text = objFilepath;
                UI_Textbox_Output.Text += "obj ready to send to hologram!\r\n";

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
                UI_Button_DisplayOff.Enabled = true;
                UI_Button_Disconnect.Enabled = true;
                //clientRx();
            }
        }

        private async void UI_Button_SendLoaded_Click(object sender, EventArgs e)
        {
            //check if connection is alive
            if (socket == null || !socket.Connected)
            {
                HandleDisconnect();
                return;
            }
            try
            {
                await SendAllAsync(socket, storedTxMessage);

                Trace.WriteLine($"sent {storedTxMessage.Length}");
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

        private void UI_ComboBox_TestBakedData_SelectedIndexChanged(object sender, EventArgs e)
        {
            ComboBox thisCombobox = (ComboBox)sender;
            if (!(sender is ComboBox))
            {
                return;
            }
            if (thisCombobox.SelectedIndex == 0)
            {//if we unselect a preloaded images, reset the loaded image to be blank
                UI_Textbox_LoadedImageName.Text = "";
                storedTxMessage = null;
                UI_Button_SendLoaded.Enabled = false;
                return;
            }
            (Int32, Int32, byte[]) backedData;
            try
            {
                backedData = testBakedDataDict[thisCombobox.Text];
            }
            catch
            {
                return;//this shouldn't run but just in case give a somewhat gracefull way to not crash
            }
            //Int32 currentFramecount = 1;//debug const
            //Int32 currentSliceCount = 16;//standard...
            //Int16 currentRPM = 100;//debug const
            storedTxMessage = MessageHeader.Build3DImageMessage(backedData.Item3, backedData.Item1, backedData.Item2, WifiTxDataType.Still3D, (sbyte)WifiTxMotor.Same);
            UI_Textbox_LoadedImageName.Text = thisCombobox.Text;
            UI_Button_SendLoaded.Enabled = true;
        }

        ///////////////////////////////////////////////////////////////////
        //Process Utils
        ///////////////////////////////////////////////////////////////////
        private async Task<byte[]> callVoxelConverterToConvert(string objFilepath)
        {
            if (voxelConverterProcess != null && !voxelConverterProcess.HasExited)
            { //Check if the process is running. Kill the process if its still running
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
            foreach (int val in voxelBytes)
            {
                UI_Textbox_Output.Text += $" {val},";
            }
            //cleanup process
            await voxelConverterProcess.WaitForExitAsync();//wait for the process to exit AFTER we have flushed STDOUT to the UI
            voxelConverterProcess.WaitForExit();
            voxelPipe.Disconnect();//close the pipe so we can reopen it later
            voxelPipe.Close();
            return voxelBytes;
        }

        private async Task callVoxelConverterToVisualize(string objFilepath)
        {
            Process process = new Process();
            process.StartInfo.FileName = voxelConverterProcessFilepath;
            process.StartInfo.ArgumentList.Clear();
            process.StartInfo.ArgumentList.Add(objFilepath);
            process.StartInfo.ArgumentList.Add("-dv");
            process.StartInfo.UseShellExecute = false;
            process.OutputDataReceived += Process_OutputDataReceived;
            process.EnableRaisingEvents = true;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.CreateNoWindow = true;
            process.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            process.Start();
            process.BeginOutputReadLine();
            string t = process.StandardOutput.ReadToEnd();
            UI_Textbox_Output.AppendText(t);

            await process.WaitForExitAsync();//wait for the process to exit AFTER we have flushed STDOUT to the UI
            process.WaitForExit();
        }

        private bool converterExecutableExists()
        {
            string converterPath = voxelConverterProcessFilepath;
            if (!File.Exists(converterPath))
            {
                MessageBox.Show($"WARNING: Converter .exe file not found. Please select the voxelConverter");
                OpenFileDialog openFileDialog = new OpenFileDialog();
                openFileDialog.Filter = "executable files (*.exe) | *.exe";
                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    converterPath = openFileDialog.FileName;
                }
                else
                {//close the program if the user doesn't want to provide an executable
                    MessageBox.Show($"WARNING: Hologram server requires the converter executable to run");
                    return false;
                }
                voxelConverterProcessFilepath = converterPath;
            }
            return true;
        }


        ///////////////////////////////////////////////////////////////////
        //Process Async/Events
        ///////////////////////////////////////////////////////////////////
        private async Task<byte[]> readAllDataInFromPipe(NamedPipeServerStream pipe)
        {
            using MemoryStream ms = new MemoryStream();
            byte[] buffer = new byte[PIPE_BUFFER_SIZE];
            while (true)
            {
                int bytesRead = await pipe.ReadAsync(buffer, 0, buffer.Length);
                if (bytesRead == 0) break;//pipe closed, exit the 
                ms.Write(buffer, 0, bytesRead);
            }
            return ms.ToArray();
        }

        private void Process_OutputDataReceived(object sender, DataReceivedEventArgs e)
        {
            if (e.Data != null)
            {
                UI_Textbox_Output.BeginInvoke(new Action(() =>
                {
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
            UI_Button_Disconnect.Enabled = false;
            UI_Button_DisplayOff.Enabled = false;
            UI_Textbox_Output.Text += "\r\nNot Connected";
        }

        private async void UI_Button_DisplayOff_Click(object sender, EventArgs e)
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
                msgHead.Version = MessageHeader.CURRENT_VERSION;
                msgHead.DataType = (sbyte)WifiTxDataType.DisplayOff;
                msgHead.FrameCount = -1;
                msgHead.SliceCount = -1;
                msgHead.PayloadBytes = -1;
                msgHead.MotorSpeedRpm = (short)WifiTxMotor.Same;
                msgHead.PayloadCrc32 = 0;

                byte[] header = msgHead.GetBytes();

                await SendAllAsync(socket, header);

                Trace.WriteLine($"sent {header.Length} bytes:\n{msgHead}");
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
    }
}
