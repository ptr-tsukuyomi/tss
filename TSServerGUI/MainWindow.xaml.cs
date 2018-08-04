using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace TSServerGUI
{
	namespace Views
	{
		/// <summary>
		/// MainWindow.xaml の相互作用ロジック
		/// </summary>
		public partial class MainWindow : Window
		{
			NIWrapper ni;

			public MainWindow()
			{
				InitializeComponent();
				ni = new NIWrapper(() => { this.WindowState = WindowState.Normal; this.ShowInTaskbar = true; this.Activate(); });
			}

			private void Window_StateChanged(object sender, EventArgs e)
			{
				if (this.WindowState == WindowState.Minimized)
				{
					this.ShowInTaskbar = false;
				}
			}

			private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
			{
				var vm = this.DataContext as ViewModels.MainWindowViewModel;
				if (vm.IsClientAvailable())
				{
					var mb = MessageBox.Show(this, "クライアントが存在します。終了しますか？", Title, MessageBoxButton.YesNo, MessageBoxImage.Exclamation,MessageBoxResult.No);
					switch (mb)
					{
						case MessageBoxResult.Yes:
							break;
						case MessageBoxResult.No:
							e.Cancel = true;
							return;
					}
				}
				vm.Close();
			}
		}
	}
}
