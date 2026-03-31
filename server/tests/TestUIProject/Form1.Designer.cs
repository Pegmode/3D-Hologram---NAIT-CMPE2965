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
            UI_Textbox_OUtput = new TextBox();
            UI_Button_RUN = new Button();
            UI_Button_Visualize = new Button();
            SuspendLayout();
            // 
            // UI_Textbox_OUtput
            // 
            UI_Textbox_OUtput.Location = new Point(208, 12);
            UI_Textbox_OUtput.Multiline = true;
            UI_Textbox_OUtput.Name = "UI_Textbox_OUtput";
            UI_Textbox_OUtput.ReadOnly = true;
            UI_Textbox_OUtput.Size = new Size(282, 277);
            UI_Textbox_OUtput.TabIndex = 0;
            // 
            // UI_Button_RUN
            // 
            UI_Button_RUN.Location = new Point(30, 56);
            UI_Button_RUN.Name = "UI_Button_RUN";
            UI_Button_RUN.Size = new Size(75, 23);
            UI_Button_RUN.TabIndex = 1;
            UI_Button_RUN.Text = "RUN!";
            UI_Button_RUN.UseVisualStyleBackColor = true;
            UI_Button_RUN.Click += UI_Button_RUN_Click;
            // 
            // UI_Button_Visualize
            // 
            UI_Button_Visualize.Location = new Point(30, 85);
            UI_Button_Visualize.Name = "UI_Button_Visualize";
            UI_Button_Visualize.Size = new Size(75, 23);
            UI_Button_Visualize.TabIndex = 2;
            UI_Button_Visualize.Text = "Visualize";
            UI_Button_Visualize.UseVisualStyleBackColor = true;
            UI_Button_Visualize.Click += UI_Button_Visualize_Click;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
            Controls.Add(UI_Button_Visualize);
            Controls.Add(UI_Button_RUN);
            Controls.Add(UI_Textbox_OUtput);
            Name = "Form1";
            Text = "Form1";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private TextBox UI_Textbox_OUtput;
        private Button UI_Button_RUN;
        private Button UI_Button_Visualize;
    }
}
