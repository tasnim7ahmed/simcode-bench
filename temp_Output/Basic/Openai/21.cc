#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment
{
public:
  Experiment ();
  void Run (std::string phyMode, double rss, uint32_t payloadSize);
  uint32_t GetReceivedPackets () const;

private:
  void ReceivePacket (Ptr<Socket> socket);
  void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval);
  void SetPositions (Ptr<Node> staNode, Ptr<Node> apNode);

  uint32_t m_receivedPackets;
};

Experiment::Experiment ()
  : m_receivedPackets (0)
{
}

void
Experiment::SetPositions (Ptr<Node> staNode, Ptr<Node> apNode)
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  NodeContainer nc;
  nc.Add (staNode);
  nc.Add (apNode);
  mobility.Install (nc);
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      m_receivedPackets++;
    }
}

void
Experiment::GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
  if (pktCount > 0)
    {
      socket->Send (Create <Packet> (pktSize));
      Simulator::Schedule (pktInterval, &Experiment::GenerateTraffic, this, socket, pktSize, pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

void
Experiment::Run (std::string phyMode, double rss, uint32_t payloadSize)
{
  m_receivedPackets = 0;

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.SetErrorRateModel ("ns3::NistErrorRateModel");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode", StringValue (phyMode));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("wifi-experiment");
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  SetPositions (wifiStaNode.Get (0), wifiApNode.Get (0));

  // Set Received Signal Strength (RSS) by adjusting transmission power and loss
  Ptr<YansWifiChannel> wifiChannel = DynamicCast<YansWifiChannel> (phy.GetChannel ());
  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
  wifiChannel->SetPropagationDelayModel (delayModel);
  Ptr<ConstantLossPropagationLossModel> lossModel = CreateObject<ConstantLossPropagationLossModel> ();
  double referenceLoss = 0.0; //dB
  lossModel->SetLoss (referenceLoss);
  wifiChannel->AddPropagationLossModel (lossModel);

  // Calculate the txPower required to achieve the given RSS (approximate, neglecting other losses)
  phy.Set ("TxPowerStart", DoubleValue (rss));
  phy.Set ("TxPowerEnd", DoubleValue (rss));
  phy.Set ("TxPowerLevels", UintegerValue (1));

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface, apNodeInterface;
  staNodeInterface = address.Assign (staDevice);
  address.NewNetwork ();
  apNodeInterface = address.Assign (apDevice);

  // UDP application configuration
  uint16_t port = 5000;
  Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice (staDevice.Get (0)->GetIfIndex ());
  socketAddr.SetPhysicalAddress (apDevice.Get (0)->GetAddress ());
  socketAddr.SetProtocol (0x0800);

  Ptr<Socket> recvSink = Socket::CreateSocket (wifiApNode.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));

  Ptr<Socket> source = Socket::CreateSocket (wifiStaNode.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (apNodeInterface.GetAddress (0), port);
  source->Connect (remote);

  // Application parameters
  uint32_t numPackets = 1000;
  Time interPacketInterval = Seconds (0.001); // 1 ms packets

  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
      Seconds (1.0), &Experiment::GenerateTraffic, this, source, payloadSize, numPackets, interPacketInterval);

  Simulator::Stop (Seconds (2.0 + numPackets * interPacketInterval.GetSeconds ()));
  Simulator::Run ();
  Simulator::Destroy ();
}

uint32_t
Experiment::GetReceivedPackets () const
{
  return m_receivedPackets;
}

int
main (int argc, char *argv[])
{
  std::string phyModes[] = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };
  double rssValues[] = {
    -60.0, -65.0, -70.0, -75.0, -80.0, -85.0, -90.0, -95.0, -100.0
  };
  uint32_t payloadSize = 1000;

  Gnuplot gnuplot ("wifi-rss-vs-pkts.eps");
  gnuplot.SetTitle ("Packets Received vs RSS for 802.11b Data Rates");
  gnuplot.SetLegend ("RSS (dBm)", "Packets Received");
  gnuplot.SetTerminal ("postscript eps enhanced color font 'Helvetica,10'");

  for (const auto& phyMode : phyModes)
    {
      Gnuplot2dDataset dataset;
      std::ostringstream label;
      label << phyMode;
      dataset.SetTitle (label.str ());
      dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

      for (double rss : rssValues)
        {
          Experiment exp;
          exp.Run (phyMode, rss, payloadSize);
          uint32_t pkts = exp.GetReceivedPackets ();
          dataset.Add (rss, pkts);
        }
      gnuplot.AddDataset (dataset);
    }

  std::ofstream plotFile ("wifi-rss-vs-pkts.plt");
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();

  return 0;
}