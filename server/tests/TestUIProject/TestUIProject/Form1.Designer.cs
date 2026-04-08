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
        private void InitializeComponent() {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            UI_Textbox_Output = new TextBox();
            UI_Button_RUN = new Button();
            UI_Button_Visualize = new Button();
            UI_Button_LoadObj = new Button();
            UI_Button_Connect = new Button();
            d = new Label();
            UI_Button_Send_Test = new Button();
            UI_Button_Disconnect = new Button();
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
            UI_Button_RUN.Size = new Size(113, 23);
            UI_Button_RUN.TabIndex = 1;
            UI_Button_RUN.Text = "Run Hologram";
            UI_Button_RUN.UseVisualStyleBackColor = true;
            UI_Button_RUN.Click += UI_Button_RUN_Click;
            // 
            // UI_Button_Visualize
            // 
            UI_Button_Visualize.Location = new Point(30, 73);
            UI_Button_Visualize.Name = "UI_Button_Visualize";
            UI_Button_Visualize.Size = new Size(113, 23);
            UI_Button_Visualize.TabIndex = 2;
            UI_Button_Visualize.Text = "Visualize";
            UI_Button_Visualize.UseVisualStyleBackColor = true;
            UI_Button_Visualize.Click += UI_Button_Visualize_Click;
            // 
            // UI_Button_LoadObj
            // 
            UI_Button_LoadObj.Location = new Point(30, 44);
            UI_Button_LoadObj.Name = "UI_Button_LoadObj";
            UI_Button_LoadObj.Size = new Size(113, 23);
            UI_Button_LoadObj.TabIndex = 3;
            UI_Button_LoadObj.Text = "Load .obj";
            UI_Button_LoadObj.UseVisualStyleBackColor = true;
            UI_Button_LoadObj.Click += UI_Button_LoadObj_Click;
            // 
            // UI_Button_Connect
            // 
            UI_Button_Connect.Location = new Point(30, 102);
            UI_Button_Connect.Name = "UI_Button_Connect";
            UI_Button_Connect.Size = new Size(113, 23);
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
            // UI_Button_Send_Test
            // 
            UI_Button_Send_Test.Enabled = false;
            UI_Button_Send_Test.Location = new Point(30, 131);
            UI_Button_Send_Test.Name = "UI_Button_Send_Test";
            UI_Button_Send_Test.Size = new Size(113, 23);
            UI_Button_Send_Test.TabIndex = 6;
            UI_Button_Send_Test.Text = "Send Test";
            UI_Button_Send_Test.UseVisualStyleBackColor = true;
            UI_Button_Send_Test.Click += UI_Button_Send_Test_Click;
            // 
            // UI_Button_Disconnect
            // 
            UI_Button_Disconnect.Enabled = false;
            UI_Button_Disconnect.Location = new Point(30, 160);
            UI_Button_Disconnect.Name = "UI_Button_Disconnect";
            UI_Button_Disconnect.Size = new Size(113, 23);
            UI_Button_Disconnect.TabIndex = 7;
            UI_Button_Disconnect.Text = "Disconnect";
            UI_Button_Disconnect.UseVisualStyleBackColor = true;
            UI_Button_Disconnect.Click += UI_Button_Disconnect_Click;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(511, 450);
            Controls.Add(UI_Button_Disconnect);
            Controls.Add(UI_Button_Send_Test);
            Controls.Add(d);
            Controls.Add(UI_Button_Connect);
            Controls.Add(UI_Button_LoadObj);
            Controls.Add(UI_Button_Visualize);
            Controls.Add(UI_Button_RUN);
            Controls.Add(UI_Textbox_Output);
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "Form1";
            Text = "3D Hologram Server";
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
        private Button UI_Button_Send_Test;
        private Button UI_Button_Disconnect;
    }
}
