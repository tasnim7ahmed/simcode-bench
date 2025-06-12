#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment
{
public:
  Experiment();
  void Run(std::string phyMode, double rss, Gnuplot2dDataset &dataset);

private:
  void ReceivePacket(Ptr<Socket> socket);
  void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval);
  void ResetCounters();
  void SetPosition(Ptr<Node> node, double x, double y, double z);

  uint32_t m_receivedPackets;
};

Experiment::Experiment()
  : m_receivedPackets(0)
{
}

void
Experiment::ResetCounters()
{
  m_receivedPackets = 0;
}

void
Experiment::SetPosition(Ptr<Node> node, double x, double y, double z)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  mobility->SetPosition(Vector(x, y, z));
}

void
Experiment::ReceivePacket(Ptr<Socket> socket)
{
  while (socket->Recv())
    {
      m_receivedPackets++;
    }
}

void
Experiment::GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
  if (pktCount > 0)
    {
      socket->Send(Create<Packet>(pktSize));
      Simulator::Schedule(pktInterval, &Experiment::GenerateTraffic, this, socket, pktSize, pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close();
    }
}

void
Experiment::Run(std::string phyMode, double rss, Gnuplot2dDataset &dataset)
{
  // Reset counters
  ResetCounters();

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Configure WiFi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  // WiFi MAC and standard
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  // Set remote station manager
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue(phyMode),
                              "ControlMode", StringValue(phyMode));

  // Configure 802.11b specific channels
  phy.Set("TxPowerStart", DoubleValue(16.0));
  phy.Set("TxPowerEnd", DoubleValue(16.0));
  phy.Set("RxSensitivity", DoubleValue(-100.0)); // ensures the range includes all RSS values

  // MAC helper
  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  // Install devices
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  // Receiver at (0,0,0), sender at (distance,0,0) -- we use 5 meters separation as generic value
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // Receiver
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));  // Sender
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

  // Set receive signal strength (RSS) by using the PropagationLossModel
  Ptr<YansWifiChannel> wifiChannel = DynamicCast<YansWifiChannel>(phy.GetChannel());
  Ptr<PropagationLossModel> loss = wifiChannel->GetPropagationLossModel();
  // To set a fixed RSS between node 0 (sender) and node 1 (receiver)
  Ptr<MatrixPropagationLossModel> matrixLoss = CreateObject<MatrixPropagationLossModel>();
  matrixLoss->SetDefaultLoss(200); // very high loss default so only defined links work
  matrixLoss->SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), -rss);
  wifiChannel->SetPropagationLossModel(matrixLoss);

  // UDP sockets
  uint16_t port = 5000;
  Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  recvSink->Bind(local);
  recvSink->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));

  Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(ifaces.GetAddress(1), port);
  source->Connect(remote);

  // Schedule traffic generation
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  Time interPacketInterval = MilliSeconds(50);
  Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                Seconds(1.0), &Experiment::GenerateTraffic, this, source, packetSize, numPackets, interPacketInterval);

  // Enable tracing (optional, can comment out if not wanted)
  //phy.EnablePcap("wifi-cc-" + phyMode, devices);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Add the result to the dataset
  dataset.Add(rss, m_receivedPackets);

  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  std::string phyMode = "DsssRate11Mbps";
  CommandLine cmd;
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse(argc, argv);

  std::vector<std::string> phyModes = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };

  // RSS values to test (in dBm, negative values)
  std::vector<double> rssValues;
  for (double rss = -100; rss <= -45; rss += 5)
    {
      rssValues.push_back(rss);
    }

  // Create Gnuplot output
  Gnuplot plot("wifi-cc.eps");
  plot.SetTerminal("postscript eps color enhanced");
  plot.SetTitle("802.11b: Received Packets vs. RSS (dBm)");
  plot.SetLegend("RSS [dBm]", "Received packets");
  plot.AppendExtra("set xrange [-100:-45]");
  plot.AppendExtra("set yrange [0:105]");

  for (auto mode : phyModes)
    {
      Gnuplot2dDataset dataset;
      dataset.SetTitle(mode);
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

      for (double rss : rssValues)
        {
          Experiment experiment;
          experiment.Run(mode, rss, dataset);
        }

      plot.AddDataset(dataset);
    }

  std::ofstream out("wifi-cc.plt");
  plot.GenerateOutput(out);

  return 0;
}