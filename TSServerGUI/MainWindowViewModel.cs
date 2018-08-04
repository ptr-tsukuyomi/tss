using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using Prism.Mvvm;

namespace TSServerGUI
{
	namespace ViewModels
	{
		class LogCallback : TSServer.Logger
		{
			Action<string, string, string> _f;
			public LogCallback(Action<string,string,string> f)
			{
				_f = f;
			}
			public void Log(string msg, string id = null, [CallerMemberName] string cause = null)
			{
				_f(msg, id, cause);
			}
		}

		public class Log
		{
			public Log(string m,string i = null,string c = null)
			{
				dt = DateTime.Now;
				Msg = m;
				ID = i;
				Cause = c;
			}

			DateTime dt;
			public String CausedDateTime { get { return dt.ToString(); }}
			public String Msg { get; }
			public String ID { get; }
			public String Cause { get; }
		}

		class LogCollection : System.Collections.ObjectModel.ObservableCollection<Log>
		{

		}

		class TunerCollection : System.Collections.ObjectModel.ObservableCollection<String>
		{

		}

		class MainWindowViewModel : BindableBase
		{
			public MainWindowViewModel() : base()
			{
				Run();
			}

			private string _title = "Hello World";
			public string Title
			{
				get { return _title; }
				set { SetProperty(ref _title, value); }
			}

			private Prism.Commands.DelegateCommand _btncmd = null;
			public Prism.Commands.DelegateCommand BtnCommand
			{
				get
				{
					if (_btncmd == null) _btncmd = new Prism.Commands.DelegateCommand(Run);
					return _btncmd;
				}
			}

			private Prism.Commands.DelegateCommand _btncmd2 = null;
			public Prism.Commands.DelegateCommand BtnCommand2
			{
				get
				{
					if (_btncmd2 == null) _btncmd2 = new Prism.Commands.DelegateCommand(Stop);
					return _btncmd2;
				}
			}

			TSServer.TSServer tss = null;
			TSServer.Logger l = null;

			LogCollection _log;
			public LogCollection Logs
			{
				get
				{
					if (_log == null) _log = new LogCollection();
					return _log;
				}
				set
				{
					_log = value;
				}
			}

			TunerCollection _tuners;
			public TunerCollection Tuners
			{
				get
				{
					if (_tuners == null) _tuners = new TunerCollection();
					return _tuners;
				}
				set
				{
					_tuners = value;
				}
			}

			System.Windows.Threading.Dispatcher d = System.Windows.Application.Current.Dispatcher;

			public bool IsRunning()
			{
				return tss != null;
			}

			public bool IsClientAvailable()
			{
				return tss.listeners.Count != 0;
			}

			public void Close()
			{
				if (IsRunning())
				{
					Stop();
				}
			}

			public void Run()
			{
				string driverdir = System.IO.Directory.GetCurrentDirectory() + "/Drivers";
				Logs.Clear();
				l = new LogCallback((a, b, c) => 
				{
					try
					{
						d.Invoke(() => { Logs.Add(new ViewModels.Log(a, b, c)); });
					} catch(TaskCanceledException)
					{

					}
				});
				tss = new TSServer.TSServer(l);
				tss.Run(driverdir, (string s) => { d.Invoke(() => { Tuners.Add(s); }); });
				Title = "Running";
			}
			
			public void Stop()
			{
				tss.Stop();
				Tuners.Clear();
				Title = "Stopped";
			}
		}
	}
}
