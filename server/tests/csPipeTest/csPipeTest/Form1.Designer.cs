namespace csPipeTest
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
            UI_Button_CreatePipe = new Button();
            UI_Textbox_Output = new TextBox();
            SuspendLayout();
            // 
            // UI_Button_CreatePipe
            // 
            UI_Button_CreatePipe.Location = new Point(12, 34);
            UI_Button_CreatePipe.Name = "UI_Button_CreatePipe";
            UI_Button_CreatePipe.Size = new Size(75, 23);
            UI_Button_CreatePipe.TabIndex = 0;
            UI_Button_CreatePipe.Text = "Create Pipe";
            UI_Button_CreatePipe.UseVisualStyleBackColor = true;
            UI_Button_CreatePipe.Click += UI_Button_CreatePipe_Click;
            // 
            // UI_Textbox_Output
            // 
            UI_Textbox_Output.Location = new Point(12, 63);
            UI_Textbox_Output.Multiline = true;
            UI_Textbox_Output.Name = "UI_Textbox_Output";
            UI_Textbox_Output.Size = new Size(325, 181);
            UI_Textbox_Output.TabIndex = 1;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
            Controls.Add(UI_Textbox_Output);
            Controls.Add(UI_Button_CreatePipe);
            Name = "Form1";
            Text = "Form1";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button UI_Button_CreatePipe;
        private TextBox UI_Textbox_Output;
    }
}
