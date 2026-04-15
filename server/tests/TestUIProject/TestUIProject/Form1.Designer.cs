namespace TestUIProject
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            UI_Textbox_Output = new TextBox();
            UI_Button_RUN = new Button();
            UI_Button_Visualize = new Button();
            UI_Button_LoadObj = new Button();
            UI_Button_Connect = new Button();
            d = new Label();
            UI_Button_Disconnect = new Button();
            UI_Textbox_LoadedImageName = new TextBox();
            label1 = new Label();
            UI_Button_SendLoaded = new Button();
            UI_ComboBox_TestBakedData = new ComboBox();
            label2 = new Label();
            UI_Button_DisplayOff = new Button();
            UI_TrackBar_EscPulseWidth = new TrackBar();
            label3 = new Label();
            UI_TextBox_Esc_Low = new TextBox();
            UI_TextBox_Esc_Value = new TextBox();
            UI_TextBox_Esc_High = new TextBox();
            UI_Button_UpdateSpeed = new Button();
            UI_Button_LoadAnimationObj = new Button();
            UI_Button_StopMotor = new Button();
            UI_TextBox_Stats = new TextBox();
            ((System.ComponentModel.ISupportInitialize)UI_TrackBar_EscPulseWidth).BeginInit();
            SuspendLayout();
            // 
            // UI_Textbox_Output
            // 
            UI_Textbox_Output.Location = new Point(164, 27);
            UI_Textbox_Output.Multiline = true;
            UI_Textbox_Output.Name = "UI_Textbox_Output";
            UI_Textbox_Output.ReadOnly = true;
            UI_Textbox_Output.ScrollBars = ScrollBars.Vertical;
            UI_Textbox_Output.Size = new Size(282, 277);
            UI_Textbox_Output.TabIndex = 0;
            // 
            // UI_Button_RUN
            // 
            UI_Button_RUN.Location = new Point(30, 15);
            UI_Button_RUN.Name = "UI_Button_RUN";
            UI_Button_RUN.Size = new Size(128, 23);
            UI_Button_RUN.TabIndex = 1;
            UI_Button_RUN.Text = "Run Hologram";
            UI_Button_RUN.UseVisualStyleBackColor = true;
            UI_Button_RUN.Click += UI_Button_RUN_Click;
            // 
            // UI_Button_Visualize
            // 
            UI_Button_Visualize.Location = new Point(30, 102);
            UI_Button_Visualize.Name = "UI_Button_Visualize";
            UI_Button_Visualize.Size = new Size(128, 23);
            UI_Button_Visualize.TabIndex = 2;
            UI_Button_Visualize.Text = "Visualize";
            UI_Button_Visualize.UseVisualStyleBackColor = true;
            UI_Button_Visualize.Click += UI_Button_Visualize_Click;
            // 
            // UI_Button_LoadObj
            // 
            UI_Button_LoadObj.Location = new Point(30, 44);
            UI_Button_LoadObj.Name = "UI_Button_LoadObj";
            UI_Button_LoadObj.Size = new Size(128, 23);
            UI_Button_LoadObj.TabIndex = 3;
            UI_Button_LoadObj.Text = "Load .obj";
            UI_Button_LoadObj.UseVisualStyleBackColor = true;
            UI_Button_LoadObj.Click += UI_Button_LoadObj_Click;
            // 
            // UI_Button_Connect
            // 
            UI_Button_Connect.Location = new Point(30, 131);
            UI_Button_Connect.Name = "UI_Button_Connect";
            UI_Button_Connect.Size = new Size(128, 23);
            UI_Button_Connect.TabIndex = 4;
            UI_Button_Connect.Text = "Connect";
            UI_Button_Connect.UseVisualStyleBackColor = true;
            UI_Button_Connect.Click += UI_Button_Connect_Click;
            // 
            // d
            // 
            d.AutoSize = true;
            d.Location = new Point(164, 9);
            d.Name = "d";
            d.Size = new Size(30, 15);
            d.TabIndex = 5;
            d.Text = "Log:";
            // 
            // UI_Button_Disconnect
            // 
            UI_Button_Disconnect.Enabled = false;
            UI_Button_Disconnect.Location = new Point(30, 189);
            UI_Button_Disconnect.Name = "UI_Button_Disconnect";
            UI_Button_Disconnect.Size = new Size(128, 23);
            UI_Button_Disconnect.TabIndex = 7;
            UI_Button_Disconnect.Text = "Disconnect";
            UI_Button_Disconnect.UseVisualStyleBackColor = true;
            UI_Button_Disconnect.Click += UI_Button_Disconnect_Click;
            // 
            // UI_Textbox_LoadedImageName
            // 
            UI_Textbox_LoadedImageName.Location = new Point(250, 342);
            UI_Textbox_LoadedImageName.Name = "UI_Textbox_LoadedImageName";
            UI_Textbox_LoadedImageName.ReadOnly = true;
            UI_Textbox_LoadedImageName.Size = new Size(196, 23);
            UI_Textbox_LoadedImageName.TabIndex = 9;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(159, 345);
            label1.Name = "label1";
            label1.Size = new Size(85, 15);
            label1.TabIndex = 10;
            label1.Text = "Loaded Image:";
            // 
            // UI_Button_SendLoaded
            // 
            UI_Button_SendLoaded.Enabled = false;
            UI_Button_SendLoaded.Location = new Point(297, 371);
            UI_Button_SendLoaded.Name = "UI_Button_SendLoaded";
            UI_Button_SendLoaded.Size = new Size(149, 23);
            UI_Button_SendLoaded.TabIndex = 11;
            UI_Button_SendLoaded.Text = "Send Loaded Image";
            UI_Button_SendLoaded.UseVisualStyleBackColor = true;
            UI_Button_SendLoaded.Click += UI_Button_SendLoaded_Click;
            // 
            // UI_ComboBox_TestBakedData
            // 
            UI_ComboBox_TestBakedData.FormattingEnabled = true;
            UI_ComboBox_TestBakedData.Location = new Point(250, 310);
            UI_ComboBox_TestBakedData.Name = "UI_ComboBox_TestBakedData";
            UI_ComboBox_TestBakedData.Size = new Size(196, 23);
            UI_ComboBox_TestBakedData.TabIndex = 12;
            UI_ComboBox_TestBakedData.SelectedIndexChanged += UI_ComboBox_TestBakedData_SelectedIndexChanged;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(159, 313);
            label2.Name = "label2";
            label2.Size = new Size(88, 15);
            label2.TabIndex = 13;
            label2.Text = "Built in images:";
            // 
            // UI_Button_DisplayOff
            // 
            UI_Button_DisplayOff.Enabled = false;
            UI_Button_DisplayOff.Location = new Point(30, 160);
            UI_Button_DisplayOff.Name = "UI_Button_DisplayOff";
            UI_Button_DisplayOff.Size = new Size(128, 23);
            UI_Button_DisplayOff.TabIndex = 14;
            UI_Button_DisplayOff.Text = "Display Off";
            UI_Button_DisplayOff.UseVisualStyleBackColor = true;
            UI_Button_DisplayOff.Click += UI_Button_DisplayOff_Click;
            // 
            // UI_TrackBar_EscPulseWidth
            // 
            UI_TrackBar_EscPulseWidth.LargeChange = 50;
            UI_TrackBar_EscPulseWidth.Location = new Point(30, 456);
            UI_TrackBar_EscPulseWidth.Maximum = 1200;
            UI_TrackBar_EscPulseWidth.Minimum = 870;
            UI_TrackBar_EscPulseWidth.Name = "UI_TrackBar_EscPulseWidth";
            UI_TrackBar_EscPulseWidth.Size = new Size(416, 45);
            UI_TrackBar_EscPulseWidth.SmallChange = 10;
            UI_TrackBar_EscPulseWidth.TabIndex = 15;
            UI_TrackBar_EscPulseWidth.TickFrequency = 5;
            UI_TrackBar_EscPulseWidth.Value = 870;
            UI_TrackBar_EscPulseWidth.ValueChanged += UI_TrackBar_EscPulseWidth_ValueChanged;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(164, 504);
            label3.Name = "label3";
            label3.Size = new Size(153, 15);
            label3.TabIndex = 16;
            label3.Text = "esc pulse width (for testing)";
            // 
            // UI_TextBox_Esc_Low
            // 
            UI_TextBox_Esc_Low.Location = new Point(18, 432);
            UI_TextBox_Esc_Low.Name = "UI_TextBox_Esc_Low";
            UI_TextBox_Esc_Low.ReadOnly = true;
            UI_TextBox_Esc_Low.Size = new Size(49, 23);
            UI_TextBox_Esc_Low.TabIndex = 17;
            UI_TextBox_Esc_Low.Text = "870us";
            // 
            // UI_TextBox_Esc_Value
            // 
            UI_TextBox_Esc_Value.Location = new Point(213, 432);
            UI_TextBox_Esc_Value.Name = "UI_TextBox_Esc_Value";
            UI_TextBox_Esc_Value.ReadOnly = true;
            UI_TextBox_Esc_Value.Size = new Size(49, 23);
            UI_TextBox_Esc_Value.TabIndex = 18;
            UI_TextBox_Esc_Value.Text = "870us";
            // 
            // UI_TextBox_Esc_High
            // 
            UI_TextBox_Esc_High.Location = new Point(397, 432);
            UI_TextBox_Esc_High.Name = "UI_TextBox_Esc_High";
            UI_TextBox_Esc_High.ReadOnly = true;
            UI_TextBox_Esc_High.Size = new Size(49, 23);
            UI_TextBox_Esc_High.TabIndex = 19;
            UI_TextBox_Esc_High.Text = "1200";
            // 
            // UI_Button_UpdateSpeed
            // 
            UI_Button_UpdateSpeed.Enabled = false;
            UI_Button_UpdateSpeed.Location = new Point(12, 500);
            UI_Button_UpdateSpeed.Name = "UI_Button_UpdateSpeed";
            UI_Button_UpdateSpeed.Size = new Size(113, 23);
            UI_Button_UpdateSpeed.TabIndex = 20;
            UI_Button_UpdateSpeed.Text = "Update Speed";
            UI_Button_UpdateSpeed.UseVisualStyleBackColor = true;
            UI_Button_UpdateSpeed.Click += UI_Button_UpdateSpeed_Click;
            // 
            // UI_Button_LoadAnimationObj
            // 
            UI_Button_LoadAnimationObj.Location = new Point(30, 73);
            UI_Button_LoadAnimationObj.Name = "UI_Button_LoadAnimationObj";
            UI_Button_LoadAnimationObj.Size = new Size(128, 23);
            UI_Button_LoadAnimationObj.TabIndex = 21;
            UI_Button_LoadAnimationObj.Text = "Load .obj Animated";
            UI_Button_LoadAnimationObj.UseVisualStyleBackColor = true;
            UI_Button_LoadAnimationObj.Click += UI_Button_LoadAnimationObj_Click;
            // 
            // UI_Button_StopMotor
            // 
            UI_Button_StopMotor.Enabled = false;
            UI_Button_StopMotor.Location = new Point(12, 394);
            UI_Button_StopMotor.Name = "UI_Button_StopMotor";
            UI_Button_StopMotor.Size = new Size(113, 23);
            UI_Button_StopMotor.TabIndex = 22;
            UI_Button_StopMotor.Text = "Stop Motor";
            UI_Button_StopMotor.UseVisualStyleBackColor = true;
            UI_Button_StopMotor.Click += UI_Button_StopMotor_Click;
            // 
            // UI_TextBox_Stats
            // 
            UI_TextBox_Stats.Location = new Point(12, 284);
            UI_TextBox_Stats.Multiline = true;
            UI_TextBox_Stats.Name = "UI_TextBox_Stats";
            UI_TextBox_Stats.ReadOnly = true;
            UI_TextBox_Stats.Size = new Size(141, 104);
            UI_TextBox_Stats.TabIndex = 23;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(478, 550);
            Controls.Add(UI_TextBox_Stats);
            Controls.Add(UI_Button_StopMotor);
            Controls.Add(UI_Button_LoadAnimationObj);
            Controls.Add(UI_Button_UpdateSpeed);
            Controls.Add(UI_TextBox_Esc_High);
            Controls.Add(UI_TextBox_Esc_Value);
            Controls.Add(UI_TextBox_Esc_Low);
            Controls.Add(label3);
            Controls.Add(UI_TrackBar_EscPulseWidth);
            Controls.Add(UI_Button_DisplayOff);
            Controls.Add(label2);
            Controls.Add(UI_ComboBox_TestBakedData);
            Controls.Add(UI_Button_SendLoaded);
            Controls.Add(label1);
            Controls.Add(UI_Textbox_LoadedImageName);
            Controls.Add(UI_Button_Disconnect);
            Controls.Add(d);
            Controls.Add(UI_Button_Connect);
            Controls.Add(UI_Button_LoadObj);
            Controls.Add(UI_Button_Visualize);
            Controls.Add(UI_Button_RUN);
            Controls.Add(UI_Textbox_Output);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "Form1";
            Text = "3D Hologram Server";
            ((System.ComponentModel.ISupportInitialize)UI_TrackBar_EscPulseWidth).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private TextBox UI_Textbox_Output;
        private Button UI_Button_RUN;
        private Button UI_Button_Visualize;
        private Button UI_Button_LoadObj;
        private Button UI_Button_Connect;
        private Label d;
        private Button UI_Button_Disconnect;
        private TextBox UI_Textbox_LoadedImageName;
        private Label label1;
        private Button UI_Button_SendLoaded;
        private ComboBox UI_ComboBox_TestBakedData;
        private Label label2;
        private Button UI_Button_DisplayOff;
        private TrackBar UI_TrackBar_EscPulseWidth;
        private Label label3;
        private TextBox UI_TextBox_Esc_Low;
        private TextBox UI_TextBox_Esc_Value;
        private TextBox UI_TextBox_Esc_High;
        private Button UI_Button_UpdateSpeed;
        private Button UI_Button_LoadAnimationObj;
        private Button UI_Button_StopMotor;
        private TextBox UI_TextBox_Stats;
    }
}
