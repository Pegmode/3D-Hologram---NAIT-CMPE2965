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
            UI_Textbox_OUtput = new TextBox();
            UI_Button_RUN = new Button();
            UI_Button_Visualize = new Button();
            UI_Button_LoadObj = new Button();
            UI_Button_Connect = new Button();
            d = new Label();
            SuspendLayout();
            // 
            // UI_Textbox_OUtput
            // 
            UI_Textbox_OUtput.Location = new Point(164, 27);
            UI_Textbox_OUtput.Multiline = true;
            UI_Textbox_OUtput.Name = "UI_Textbox_OUtput";
            UI_Textbox_OUtput.ReadOnly = true;
            UI_Textbox_OUtput.ScrollBars = ScrollBars.Vertical;
            UI_Textbox_OUtput.Size = new Size(282, 277);
            UI_Textbox_OUtput.TabIndex = 0;
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
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(511, 450);
            Controls.Add(d);
            Controls.Add(UI_Button_Connect);
            Controls.Add(UI_Button_LoadObj);
            Controls.Add(UI_Button_Visualize);
            Controls.Add(UI_Button_RUN);
            Controls.Add(UI_Textbox_OUtput);
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "Form1";
            Text = "3D Hologram Server";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private TextBox UI_Textbox_OUtput;
        private Button UI_Button_RUN;
        private Button UI_Button_Visualize;
        private Button UI_Button_LoadObj;
        private Button UI_Button_Connect;
        private Label d;
    }
}
