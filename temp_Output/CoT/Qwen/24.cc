#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ErrorModelComparison");

class ErrorModelTester {
public:
  ErrorModelTester ();
  void Run (uint32_t frameSize, uint32_t mcsStart, uint32_t mcsStep, uint32_t mcsEnd,
            double snrStart, double snrStep, double snrEnd,
            bool enableNist, bool enableYans, bool enableTable);

private:
  void SetupSimulation (Ptr<WifiNetDevice> sender, Ptr<WifiNetDevice> receiver,
                        YansWifiChannelHelper channelHelper,
                        const std::string& errorModelType, uint8_t mcs, double snr);
  void SendPacket (Ptr<Socket> socket, Ipv4Address destination, uint32_t packetSize);
  void ReceivePacket (Ptr<Socket> socket);
  uint32_t m_receivedPackets;
};

ErrorModelTester::ErrorModelTester ()
  : m_receivedPackets (0)
{
}

void
ErrorModelTester::Run (uint32_t frameSize, uint32_t mcsStart, uint32_t mcsStep, uint32_t mcsEnd,
                       double snrStart, double snrStep, double snrEnd,
                       bool enableNist, bool enableYans, bool enableTable)
{
  NodeContainer nodes;
  nodes.Create (2);

  YansWifiPhyHelper phy;
  WifiMacHelper mac;
  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();

  NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);

  Ptr<WifiNetDevice> sender = DynamicCast<WifiNetDevice> (devices.Get (0));
  Ptr<WifiNetDevice> receiver = DynamicCast<WifiNetDevice> (devices.Get (1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  Gnuplot2dDataset nistDataset ("Nist");
  nistDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  Gnuplot2dDataset yansDataset ("Yans");
  yansDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  Gnuplot2dDataset tableDataset ("Table");
  tableDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
    {
      if (enableNist)
        {
          wifi.SetStandard (WIFI_STANDARD_80211n_5MHZ);
          wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("HtMcs" + std::to_string (mcsStart)),
                                        "ControlMode", StringValue ("HtMcs0"));
          phy.Set ("TxPowerStart", DoubleValue (10.0));
          phy.Set ("TxPowerEnd", DoubleValue (10.0));
          phy.Set ("RxGain", DoubleValue (0.0));
          phy.Set ("ChannelSettings", StringValue ("{0, 0, BAND_5GHZ, 0}"));
          phy.SetErrorRateModel ("ns3::NistErrorRateModel");
          SetupSimulation (sender, receiver, channel, "Nist", mcsStart, snr);
          double fer = static_cast<double> (m_receivedPackets) / 100;
          nistDataset.Add (snr, fer);
        }

      if (enableYans)
        {
          wifi.SetStandard (WIFI_STANDARD_80211n_5MHZ);
          wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("HtMcs" + std::to_string (mcsStart)),
                                        "ControlMode", StringValue ("HtMcs0"));
          phy.Set ("TxPowerStart", DoubleValue (10.0));
          phy.Set ("TxPowerEnd", DoubleValue (10.0));
          phy.Set ("RxGain", DoubleValue (0.0));
          phy.Set ("ChannelSettings", StringValue ("{0, 0, BAND_5GHZ, 0}"));
          phy.SetErrorRateModel ("ns3::YansErrorRateModel");
          SetupSimulation (sender, receiver, channel, "Yans", mcsStart, snr);
          double fer = static_cast<double> (m_receivedPackets) / 100;
          yansDataset.Add (snr, fer);
        }

      if (enableTable)
        {
          wifi.SetStandard (WIFI_STANDARD_80211n_5MHZ);
          wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("HtMcs" + std::to_string (mcsStart)),
                                        "ControlMode", StringValue ("HtMcs0"));
          phy.Set ("TxPowerStart", DoubleValue (10.0));
          phy.Set ("TxPowerEnd", DoubleValue (10.0));
          phy.Set ("RxGain", DoubleValue (0.0));
          phy.Set ("ChannelSettings", StringValue ("{0, 0, BAND_5GHZ, 0}"));
          phy.SetErrorRateModel ("ns3::TableBasedErrorRateModel");
          SetupSimulation (sender, receiver, channel, "Table", mcsStart, snr);
          double fer = static_cast<double> (m_receivedPackets) / 100;
          tableDataset.Add (snr, fer);
        }
    }

  Gnuplot plot ("fer-vs-snr.eps");
  plot.SetTerminal ("postscript eps color enhanced");
  plot.SetTitle ("Frame Error Rate vs SNR for Different MCS Values");
  plot.SetLegend ("SNR (dB)", "Frame Error Rate (FER)");
  plot.SetExtra ("set logscale y\nset key bottom right");

  if (enableNist) plot.AddDataset (nistDataset);
  if (enableYans) plot.AddDataset (yansDataset);
  if (enableTable) plot.AddDataset (tableDataset);

  std::ofstream plotFile ("fer-vs-snr.plot");
  plot.GenerateOutput (plotFile);
}

void
ErrorModelTester::SetupSimulation (Ptr<WifiNetDevice> sender, Ptr<WifiNetDevice> receiver,
                                   YansWifiChannelHelper channelHelper,
                                   const std::string& errorModelType, uint8_t mcs, double snr)
{
  m_receivedPackets = 0;

  channelHelper.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  channelHelper.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper phy;
  phy.SetChannel (channelHelper.Create ());
  phy.Set ("TxPowerStart", DoubleValue (10.0));
  phy.Set ("TxPowerEnd", DoubleValue (10.0));
  phy.Set ("RxGain", DoubleValue (0.0));
  phy.Set ("CcaEdThreshold", DoubleValue (-95.0));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-95.0 + 5.0));
  phy.Set ("ChannelSettings", StringValue ("{0, 0, BAND_5GHZ, 0}"));

  if (errorModelType == "Nist")
    {
      phy.SetErrorRateModel ("ns3::NistErrorRateModel");
    }
  else if (errorModelType == "Yans")
    {
      phy.SetErrorRateModel ("ns3::YansErrorRateModel");
    }
  else if (errorModelType == "Table")
    {
      phy.SetErrorRateModel ("ns3::TableBasedErrorRateModel");
    }

  WifiMacHelper mac;
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("test")));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5MHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("HtMcs" + std::to_string (mcs)),
                                "ControlMode", StringValue ("HtMcs0"));

  NetDeviceContainer devices = wifi.Install (phy, mac, NodeContainer (sender->GetNode ()));

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (Ssid ("test")));
  devices = wifi.Install (phy, mac, NodeContainer (receiver->GetNode ()));

  sender->SetIfIndex (1);
  receiver->SetIfIndex (1);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> senderSocket = Socket::CreateSocket (sender->GetNode (), tid);
  Ptr<Socket> receiverSocket = Socket::CreateSocket (receiver->GetNode (), tid);

  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  receiverSocket->Bind (local);
  receiverSocket->SetRecvCallback (MakeCallback (&ErrorModelTester::ReceivePacket, this));

  Simulator::Schedule (Seconds (1.0), &ErrorModelTester::SendPacket, this, senderSocket, interfaces.GetAddress (1), 1024);
  Simulator::Run ();
  Simulator::Destroy ();
}

void
ErrorModelTester::SendPacket (Ptr<Socket> socket, Ipv4Address destination, uint32_t packetSize)
{
  Ptr<Packet> packet = Create<Packet> (packetSize);
  socket->SendTo (packet, 0, InetSocketAddress (destination, 80));
}

void
ErrorModelTester::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv ()))
    {
      m_receivedPackets++;
    }
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1024;
  uint32_t mcsStart = 0;
  uint32_t mcsStep = 1;
  uint32_t mcsEnd = 7;
  double snrStart = -10;
  double snrStep = 2;
  double snrEnd = 30;
  bool enableNist = true;
  bool enableYans = true;
  bool enableTable = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("mcsStart", "Starting MCS value", mcsStart);
  cmd.AddValue ("mcsStep", "MCS step value", mcsStep);
  cmd.AddValue ("mcsEnd", "Ending MCS value", mcsEnd);
  cmd.AddValue ("snrStart", "Starting SNR value", snrStart);
  cmd.AddValue ("snrStep", "SNR step value", snrStep);
  cmd.AddValue ("snrEnd", "Ending SNR value", snrEnd);
  cmd.AddValue ("enableNist", "Enable Nist error model", enableNist);
  cmd.AddValue ("enableYans", "Enable Yans error model", enableYans);
  cmd.AddValue ("enableTable", "Enable Table-based error model", enableTable);
  cmd.Parse (argc, argv);

  ErrorModelTester tester;
  tester.Run (frameSize, mcsStart, mcsStep, mcsEnd, snrStart, snrStep, snrEnd, enableNist, enableYans, enableTable);

  return 0;
}