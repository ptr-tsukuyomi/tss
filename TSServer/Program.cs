using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Net.Sockets;
using System.Net;

using B25DecoderWrapper;
using BonDriverWrapper;
using System.Xml.XPath;
using System.Runtime.CompilerServices;

namespace TSServer
{
	public class TSListener
	{
		public TcpClient VideoConnection;
		public TcpClient ControlConnection;
		public System.IO.StreamReader ControlConnectionStreamReader;
		public string GUID;

		public String HostName;
		public String ProcessName;
		public int ProcessID;
	}

	public class BonDriverWithPrivilege : BonDriver
	{
		private List<String> PrivilegeClients = new List<String>();

		private object syncobj = new object();

		public void AddPrivilegeClient(String name)
		{
			lock (syncobj)
			{
				PrivilegeClients.Add(name);
			}
		}

		public void RemovePrivilegeClient(String name)
		{
			lock(syncobj)
			{
				PrivilegeClients.Remove(name);
			}
		}

		public bool IsPrivilegeClientAvailable { get { return PrivilegeClients.Count != 0; } }

		public BonDriverWithPrivilege(string path) : base(path)
		{
		}
	}

	public class Context
	{
		public TSServer cls;
		public object obj;
		public Context(TSServer s,Object o)
		{
			cls = s;
			obj = o;
		}
	}

	public interface Logger
	{
		void Log(string msg, string id = null, [System.Runtime.CompilerServices.CallerMemberName] string cause = null);
	}

	public class ConsoleLogger : Logger
	{
		static object console_syncobj = new object();

		public void Log(string msg, string id = null, [System.Runtime.CompilerServices.CallerMemberName] string cause = null)
		{
			lock (console_syncobj)
			{
				Console.ForegroundColor = ConsoleColor.DarkGray;
				Console.Write(String.Format("[{0}] ", DateTime.Now));
				Console.ResetColor();
				if (id != null)
				{
					Console.Write("<");
					Console.ForegroundColor = ConsoleColor.Green;
					Console.Write(String.Format("{0}", id));
					Console.ResetColor();
					Console.Write("> ");
				}
				Console.WriteLine(msg);
			}
		}
	}



	public class TSServer
	{
		public Dictionary<string, BonDriverWithPrivilege> drivers;

		Thread ControlConnectionThread;
		Thread VideoConnectionThread;
		public Dictionary<string, TSListener> listeners;
		public Dictionary<string, Thread> TunerThreads;
		public Dictionary<string, List<TSListener>> TunerListenerList;

		int cport = 23600;
		int vport = 23601;
		const int buffersize = 188 * 1024;

		bool enable_b25dec = true;

		TcpListener ControlTcpListener = null;
		TcpListener VideoTcpListener = null;

		Logger logger = null;

		public TSServer(Logger l = null)
		{
			if (l == null) logger = new ConsoleLogger();
			else logger = l;
		}

		public void Run(String driverdir, Action<string> driverfoundcallback = null)
		{
			drivers = new Dictionary<string, BonDriverWithPrivilege>();
			listeners = new Dictionary<string, TSListener>();
			TunerThreads = new Dictionary<string, Thread>();
			TunerListenerList = new Dictionary<string, List<TSListener>>();

			var driverfilenames = from filename in System.IO.Directory.EnumerateFiles(driverdir)
								  where filename.Substring(filename.Length - 4) == ".dll" && filename.IndexOf("BonDriver_File") == -1
								  select filename;

			foreach (string e in driverfilenames)
			{
				try
				{
					BonDriverWithPrivilege d = new BonDriverWithPrivilege(e);
					string dname = e.Substring(e.LastIndexOf(@"\") + 1);
					Console.WriteLine(string.Format("Path: {0}, Name: {1}", e, dname));
					
					if (!d.OpenTuner())
					{
						System.Threading.Thread.Sleep(500);
						if (!d.OpenTuner())
						{
							d.Release();
							Console.WriteLine(string.Format("Failed: {0}", dname));
							continue;
						}
					}
					drivers[dname] = d;
					TunerListenerList[dname] = new List<TSListener>();
					Thread tt = new Thread(TunerThread);
					TunerThreads.Add(dname, tt);
					tt.Start(new Context(this,dname));
					driverfoundcallback?.Invoke(dname);
				}
				catch (Exception ex)
				{

				}
			}

			// BonDriver 読み込み完了
			ControlTcpListener = new TcpListener(IPAddress.Any, cport);
			ControlConnectionThread = new Thread(ControlConnectionAccept);
			ControlConnectionThread.Start(new Context(this,ControlTcpListener));
			VideoTcpListener = new TcpListener(IPAddress.Any, vport);
			VideoConnectionThread = new Thread(VideoConnectionAccept);
			VideoConnectionThread.Start(new Context(this,VideoTcpListener));

			Console.WriteLine("Running.");
		}

		public void Stop()
		{
			Console.WriteLine("Closing...");

			closing = true;
			if (ControlTcpListener != null)
			{
				ControlTcpListener.Stop();
				VideoTcpListener.Stop();
				ControlTcpListener = null;
				VideoTcpListener = null;
			}

			foreach (var l in listeners)
			{
				l.Value.ControlConnection.Close();
			}
			listeners.Clear();

			foreach (var e in drivers)
			{
				TunerThreads[e.Key].Join();
				e.Value.CloseTuner();
				e.Value.Release();
			}
			drivers.Clear();
			TunerThreads.Clear();

			Console.WriteLine("Closed.");
		}

		bool closing = false;

		static void TunerThread(object ctx)
		{
			Context c = (Context)ctx;
			TSServer s = c.cls;

			string tname = (string)c.obj;
			BonDriver d = s.drivers[tname];
			var buffer = new Byte[buffersize];
			var decbuffer = new Byte[buffersize];

			B25Decoder b25dec = null;
			if (s.enable_b25dec)
			{
				b25dec = new B25Decoder();
				if (!b25dec.Initialize())
				{
					s.enable_b25dec = false;
				} else
				{
					s.Log(tname, "B25 Decode feture is now enabled!");
				}
			}

			while (!s.closing)
			{
				try
				{
					var l = s.TunerListenerList[tname];
					var read = d.GetTsStream(buffer, 0, (uint)buffer.Length);

					if (l.Count == 0)
					{
						System.Threading.Thread.Sleep(10);
						continue;
					}

					uint remain = 0;

					do
					{
						UInt32 decread = 0;
						bool success_b25dec = true;

						if (s.enable_b25dec)
						{
							try
							{
								remain = b25dec.Decode(buffer, read, decbuffer, (uint)decbuffer.Length, out decread);
							}
							catch (Exception)
							{
								success_b25dec = false;
							}

						}

						foreach (var e in l)
						{
							try
							{
								if (e.VideoConnection != null)
								{
									if (s.enable_b25dec && success_b25dec)
									{
										e.VideoConnection.GetStream().Write(decbuffer, 0, (int)decread);
									}
									else
									{
										e.VideoConnection.GetStream().Write(buffer, 0, (int)read);
									}
								}
							}
							catch (System.IO.IOException ex)
							{
								s.Log(e.GUID, "Video connection has been disconnected.");
								if (e.ControlConnection != null)
								{
									e.ControlConnection.Close();
									e.ControlConnectionStreamReader.Close();
								}
								e.VideoConnection = null;
							}
							catch (InvalidOperationException)
							{
								s.Log(e.GUID, "Video connection has been disconnected.");
								if (e.ControlConnection != null)
								{
									e.ControlConnection.Close();
									e.ControlConnectionStreamReader.Close();
								}
								e.VideoConnection = null;
							}
						}
					} while (s.enable_b25dec && remain != 0);
				}
				catch (Exception e)
				{

				}
			}
			if (b25dec != null) b25dec.Release();
		}

		static void ControlConnectionAccept(object ctx)
		{
			Context c = (Context)ctx;
			TSServer s = c.cls;

			s.Log(null, "Begin accepting control connection.");

			var tl = (TcpListener)c.obj;
			tl.Start();
			try
			{
				while (true)
				{
					var tc = tl.AcceptTcpClient();
					var t = new Thread(ConnectionProc);
					t.Start(new Context(s,tc));
				}
			}
			catch (SocketException)
			{
				s.Log(null, "End accepting control connection.");
			}
		}

		static void VideoConnectionAccept(object ctx)
		{
			Context c = (Context)ctx;
			TSServer s = c.cls;

			s.Log(null, String.Format("Begin accepting video connection."));
			var tl = (TcpListener)c.obj;
			tl.Start();
			try
			{
				while (true)
				{
					var tc = tl.AcceptTcpClient();
					var ns = tc.GetStream();
					var b = new byte[40];
					ns.BeginRead(b, 0, b.Length, (IAsyncResult ar) =>
					{
						ns.EndRead(ar);
						var guid = Encoding.ASCII.GetString(b).Substring(0, 36);
						try
						{
							s.listeners[guid].VideoConnection = tc;
						}
						catch (KeyNotFoundException)
						{
							s.Log(null, String.Format("Video connection has arrived, but key is not found: {0}", guid));
						}
					}, null);
				}
			}
			catch (SocketException)
			{
				s.Log(null, String.Format("End accepting video connection."));
			}
		}

		static void ConnectionProc(object ctx)
		{
			Context c = (Context)ctx;
			TSServer s = c.cls;

			var guid = System.Guid.NewGuid().ToString();
			s.Log(guid, "Control connection has established.");

			var tc = (TcpClient)c.obj;
			var ns = tc.GetStream();
			var sr = new System.IO.StreamReader(ns);
			var sw = new System.IO.StreamWriter(ns);
			sw.AutoFlush = true;
			var listener = new TSListener() { ControlConnection = tc, GUID = guid, ControlConnectionStreamReader = sr };
			s.listeners[guid] = listener;
			sw.WriteLine(guid);

			s.Log(guid, "Send GUID to client.");

			var begin = DateTime.Now;
			while (listener.VideoConnection == null)
			{
				if (DateTime.Now - begin > new TimeSpan(0, 0, 5))
				{
					tc.Close();
					s.listeners.Remove(guid);
					s.Log(guid, "Waiting Video connection timed out.");
					return;
				}
				System.Threading.Thread.Sleep(1);
			}
			listener.VideoConnection.SendTimeout = 500;
			s.Log(guid, "Video connection has been established.");

			BonDriverWithPrivilege driver = null;
			string lasttunername = null;
			bool hasPrivilege = false;

			try
			{
				while (true)
				{
					var msg = ReadToMessageEnd(sr);
					if (String.IsNullOrEmpty(msg)) break;
					//s.Log(guid, String.Format("Message has been arrived:\n{0}", msg));

					var xdoc = System.Xml.Linq.XDocument.Parse(msg);
					string cmd = xdoc.XPathSelectElement("/command/name").Value;
					var args = from e in xdoc.XPathSelectElements("/command/args/arg") select e;

					switch (cmd)
					{
						case "OpenTuner":
							{
								var name = xdoc.XPathSelectElement("/command/args/arg[@name=\"Name\"]");
								if (name == null)
								{
									SendInvalidParameters(sw);
								}
								else
								{
									if (s.drivers.ContainsKey(name.Value))
									{
										driver = s.drivers[name.Value];
										//TunerListenerList[name.Value].Add(listener);
										lasttunername = name.Value;

										var hostname = xdoc.XPathSelectElement("/command/args/arg[@name=\"HostName\"]");
										var processname = xdoc.XPathSelectElement("/command/args/arg[@name=\"ProcessName\"]");
										var processid = xdoc.XPathSelectElement("/command/args/arg[@name=\"ProcessID\"]");

										listener.HostName = hostname == null ? null : hostname.Value;
										listener.ProcessName = processname == null ? null : processname.Value;
										if (processid != null)
										{
											int pid;
											if (int.TryParse(processid.Value, out pid))
											{
												listener.ProcessID = pid;
											}
										}

										sw.WriteLine("<response status=\"200\" />\r\n");
										var priv = xdoc.XPathSelectElement("/command/args/arg[@name=\"Privilege\"]");
										if (priv != null)
										{
											hasPrivilege = priv.Value == "True";
											if (hasPrivilege) driver.AddPrivilegeClient(guid);
										}
										s.Log(guid, String.Format("Open tuner {0}{1} from {2}:{3}({4}).", name.Value, hasPrivilege ? " with privilege" : "", listener.HostName, listener.ProcessName, listener.ProcessID));
									}
									else
									{
										sw.WriteLine("<response status=\"402\">Invalid tuner name.</response>\r\n");
									}
								}
								break;
							}
						case "SetChannel":
							{
								UInt32 space, channel;
								var success = true;

								var e_space = xdoc.XPathSelectElement("/command/args/arg[@name=\"Space\"]");
								var e_channel = xdoc.XPathSelectElement("/command/args/arg[@name=\"Channel\"]");
								if (e_space == null || e_channel == null)
								{
									SendInvalidParameters(sw);
									break;
								}

								success |= UInt32.TryParse(e_space.Value, out space);
								success |= UInt32.TryParse(e_channel.Value, out channel);
								if (!success) { SendInvalidParameters(sw); break; }

								if (driver == null) { SendTunerIsNotOpened(sw); break; }

								s.Log(guid, String.Format("Set channel. (space: {0}, channel: {1})", space, channel));

								var cursp = driver.GetCurSpace();
								var curch = driver.GetCurChannel();

								if (space == cursp && channel == curch)
								{
									sw.WriteLine(String.Format("<response status=\"200\">{0}</response>\r\n", true));
								}
								else
								{
									if (!hasPrivilege && driver.IsPrivilegeClientAvailable)
									{
										s.Log(guid, "\"Set channel\" is denied because privilege client is available.");
										sw.WriteLine(String.Format("<response status=\"405\">Privilege client is available.</response>\r\n"));
									}
									else
									{
										var result = driver.SetChannel(space, channel);
										sw.WriteLine(String.Format("<response status=\"200\">{0}</response>\r\n", result));
									}
								}

								if (!s.TunerListenerList[lasttunername].Contains(listener))
								{
									s.TunerListenerList[lasttunername].Add(listener);
									s.Log(guid, "Add Listener to TLL.");
								}

								break;
							}
						case "CloseTuner":
							// nothing to do
							// you haven't to do anything.
							break;
						case "GetSignalLevel":
							{
								if (driver == null) { SendTunerIsNotOpened(sw); break; }

								var siglevel = driver.GetSignalLevel();
								sw.WriteLine(String.Format("<response status=\"200\">{0}</response>\r\n", siglevel));
								s.Log(guid, String.Format("GetSignalLevel: {0}", siglevel));
								break;
							}
						case "EnumTuningSpace":
							{
								var e_space = xdoc.XPathSelectElement("/command/args/arg[@name=\"Space\"]");
								if (e_space == null) { SendInvalidParameters(sw); break; }

								UInt32 space;
								if (!UInt32.TryParse(e_space.Value, out space)) { SendInvalidParameters(sw); break; }

								if (driver == null) { SendTunerIsNotOpened(sw); break; }

								s.Log(guid, String.Format("EnumTuningSpace. (space: {0})", space));
								var result = driver.EnumTuningSpace(space);
								sw.WriteLine(String.Format("<response status=\"{0}\">{1}</response>\r\n", result == null ? 404 : 200, result));
								break;
							}
						case "EnumChannelName":
							{
								UInt32 space, channel;
								var success = true;

								var e_space = xdoc.XPathSelectElement("/command/args/arg[@name=\"Space\"]");
								var e_channel = xdoc.XPathSelectElement("/command/args/arg[@name=\"Channel\"]");
								if (e_space == null || e_channel == null) { SendInvalidParameters(sw); break; }

								success |= UInt32.TryParse(e_space.Value, out space);
								success |= UInt32.TryParse(e_channel.Value, out channel);
								if (!success) { SendInvalidParameters(sw); break; }

								if (driver == null) { SendTunerIsNotOpened(sw); break; }

								s.Log(guid, String.Format("EnumChannelName. (space: {0}, channel: {1})", space, channel));

								var result = driver.EnumChannelName(space, channel);
								sw.WriteLine(String.Format("<response status=\"{0}\">{1}</response>\r\n", result == null ? 404 : 200, result));
								break;
							}
						case "GetCurSpace":
							{
								if (driver == null) { SendTunerIsNotOpened(sw); break; }

								var csp = driver.GetCurSpace();
								sw.WriteLine(String.Format("<response status=\"200\">{0}</response>\r\n", csp));
								s.Log(guid, String.Format("GetCurSpace: {0}", csp));
								break;
							}
						case "GetCurChannel":
							{
								if (driver == null) { SendTunerIsNotOpened(sw); break; }

								var cch = driver.GetCurChannel();
								sw.WriteLine(String.Format("<response status=\"200\">{0}</response>\r\n", cch));
								s.Log(guid, String.Format("GetCurChannel: {0}", cch));
								break;
							}
						default:
							// invalid command
							sw.WriteLine(String.Format("<response status=\"499\">Invalid command.</response>\r\n"));
							s.Log(guid, "Invalid command.");
							break;
					}
				}
			}
			catch (System.IO.IOException)
			{

			}
			catch (System.ObjectDisposedException)
			{

			}
			finally
			{
				s.Log(guid, "Control connection has been disconnected.");

				if (listener.VideoConnection != null)
				{
					s.Log(guid, "Video connection has been disconnected.");
					listener.VideoConnection.Close();
					listener.VideoConnection = null;
				}
				if (driver != null)
				{
					if (hasPrivilege)
					{
						driver.RemovePrivilegeClient(guid);
					}
					if (s.TunerListenerList[lasttunername].Contains(listener))
					{
						s.TunerListenerList[lasttunername].Remove(listener);
						s.Log(guid, "Remove Listener from TLL.");
					}
				}
				s.listeners.Remove(guid);
			}
		}

		/* command and result syntax
			<command>
				<name>abc</name>
				<args>
					<arg name="...">...</arg>
					...
				</args>
			</command>

			<response>
			...
			</response>
		 */

		static void SendInvalidParameters(System.IO.StreamWriter sw)
		{
			sw.WriteLine("<response status=\"401\">Invalid parameters.</response>\r\n");
		}

		static void SendTunerIsNotOpened(System.IO.StreamWriter sw)
		{
			sw.WriteLine("<response status=\"403\">Tuner is not opened</response>\r\n");
		}

		static string ReadToMessageEnd(System.IO.StreamReader sr)
		{
			string result = "";

			while (true)
			{
				var s = sr.ReadLine();
				if (String.IsNullOrEmpty(s)) break;
				result += s + "\r\n";
			}

			return result;
		}

		static object console_syncobj = new object();
		static object debug_syncobj = new object();

		//void Log(String msg, [CallerMemberName] string cause = null)
		//{
		//	logger.Log(msg, null, cause);

		//	var output = String.Format("[{0}] {1}", DateTime.Now, msg);

		//	lock (debug_syncobj)
		//	{
		//		System.Diagnostics.Debug.WriteLine(output);
		//	}
		//}

		void Log(String GUID, String msg, [CallerMemberName] string cause = null)
		{
			logger.Log(msg, GUID, cause);

			var output = String.Format("[{0}] {1}{2}", DateTime.Now, (GUID != null ? String.Format("<{0}> ",GUID) : ""), msg);

			lock (debug_syncobj)
			{
				System.Diagnostics.Debug.WriteLine(output);
			}
		}
	}

	public class Program
	{
		static public void Main(string[] args)
		{
			TSServer s = new TSServer();
			string driverdir = System.IO.Directory.GetCurrentDirectory() + "/Drivers";
			s.Run(driverdir);
			Console.ReadKey();
			s.Stop();
			Console.ReadKey();
		}
	}
}
