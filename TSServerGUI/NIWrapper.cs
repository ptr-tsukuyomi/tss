using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace TSServerGUI
{
	public partial class NIWrapper : Component
	{
		Action dc;
		public NIWrapper(Action onDoubleClick)
		{
			InitializeComponent();
			dc = onDoubleClick;
		}

		public NIWrapper(IContainer container)
		{
			container.Add(this);

			InitializeComponent();
		}

		private void notifyIcon1_MouseDoubleClick(object sender, System.Windows.Forms.MouseEventArgs e)
		{
			dc();
		}
	}
}
