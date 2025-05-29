#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

struct ReceivedPacketInfo
{
  uint32_t rxBytes;
  uint32_t rxPackets;
};

class UdpSinkStatsHelper : public Application
{
public:
  UdpSinkStatsHelper () : m_rxBytes (0), m_rxPackets (0) {}
  void Setup (Ptr<Socket> socket) { m_socket = socket; }
  uint32_t GetRxBytes () const { return m_rxBytes; }
  uint32_t GetRxPackets () const { return m_rxPackets; }
private:
  virtual void StartApplication () override
  {
    m_socket->SetRecvCallback (MakeCallback (&UdpSinkStatsHelper::HandleRead, this));
  }
  virtual void StopApplication () override
  {
    if (m_socket)
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv ()))
    {
      m_rxBytes += packet->GetSize ();
      m_rxPackets++;
    }
  }
  Ptr<Socket> m_socket;
  uint32_t m_rxBytes;
  uint32_t m_rxPackets;
};

int main (int argc, char *argv[])
{
  // Default simulation parameters
  double simTime = 200.0;
  uint32_t numNodes = 50;
  double nodeSpeed = 20.0;
  double pauseTime = 0.0;
  double areaX = 1500.0;
  double areaY = 300.0;
  double txPowerDbm = 7.5;
  uint32_t nFlows = 10;
  std::string routingProt = "DSDV";
  std::string csvFilename = "manet-results.csv";
  bool enableMobilityTrace = false;
  bool enableFlowMonitor = true;
  uint16_t portBase = 9000;

  CommandLine cmd;
  cmd.AddValue ("protocol", "Routing protocol: DSDV | AODV | OLSR | DSR", routingProt);
  cmd.AddValue ("nNodes", "Number of nodes", numNodes);
  cmd.AddValue ("speed", "Node speed in m/s", nodeSpeed);
  cmd.AddValue ("pause", "Pause time for mobility in s", pauseTime);
  cmd.AddValue ("areaX", "Area length in meters (X-axis)", areaX);
  cmd.AddValue ("areaY", "Area width in meters (Y-axis)", areaY);
  cmd.AddValue ("txPower", "Wifi transmit power in dBm", txPowerDbm);
  cmd.AddValue ("flows", "Number of UDP flows", nFlows);
  cmd.AddValue ("outfile", "CSV filename for reception results", csvFilename);
  cmd.AddValue ("mobilityTrace", "Enable mobility tracing", enableMobilityTrace);
  cmd.AddValue ("flowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  SeedManager::SetSeed (12345);

  NodeContainer nodes;
  nodes.Create (numNodes);

  // WiFi configuration (802.11b, Adhoc)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));
  wifiPhy.Set ("RxGain", DoubleValue (0.0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-96.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-99.0));

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility: RandomWaypoint
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
      "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(nodeSpeed) + "]"),
      "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string(pauseTime) + "]"),
      "PositionAllocator", PointerValue (
         CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
             "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaX) + "]"),
             "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaY) + "]")
         )
      )
  );
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaX) + "]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaY) + "]"));
  mobility.Install (nodes);

  if (enableMobilityTrace)
    {
      mobility.EnableAsciiAll (Create<OutputStreamWrapper> ("mobility-trace.tr", std::ios::out));
    }

  // Internet stack and MANET routing protocol selection
  InternetStackHelper internet;
  if (routingProt == "OLSR" || routingProt == "olsr")
    {
      OlsrHelper olsr;
      internet.SetRoutingHelper (olsr);
    }
  else if (routingProt == "AODV" || routingProt == "aodv")
    {
      AodvHelper aodv;
      internet.SetRoutingHelper (aodv);
    }
  else if (routingProt == "DSDV" || routingProt == "dsdv")
    {
      DsdvHelper dsdv;
      internet.SetRoutingHelper (dsdv);
    }
  else if (routingProt == "DSR" || routingProt == "dsr")
    {
      DsrHelper dsr;
      DsrMainHelper dsrMain;
      internet.Install (nodes); // must not set dsr as internet.SetRoutingHelper!
      dsrMain.Install (dsr, nodes);
    }
  if (!(routingProt == "DSR" || routingProt == "dsr"))
    {
      internet.Install (nodes);
    }

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create UDP app pairs
  Ptr<UniformRandomVariable> randStart = CreateObject<UniformRandomVariable> ();
  randStart->SetAttribute ("Min", DoubleValue (50.0));
  randStart->SetAttribute ("Max", DoubleValue (51.0));
  Ptr<UniformRandomVariable> randNode = CreateObject<UniformRandomVariable> ();

  std::vector<Ptr<UdpSinkStatsHelper>> sinkStatsList;
  ApplicationContainer sourceApps, sinkApps;

  std::set<uint32_t> usedSinkNodes;
  for (uint32_t i = 0; i < nFlows; ++i)
    {
      // Random unique pair selection (no self-loop, no repeated destinations)
      uint32_t sinkIdx, srcIdx;
      do
        {
          sinkIdx = randNode->GetInteger (0, numNodes - 1);
        }
      while (usedSinkNodes.find (sinkIdx) != usedSinkNodes.end());
      usedSinkNodes.insert (sinkIdx);
      do
        {
          srcIdx = randNode->GetInteger (0, numNodes - 1);
        }
      while (srcIdx == sinkIdx);

      // UDP sink
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (sinkIdx), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), portBase + i);
      recvSink->Bind (local);

      Ptr<UdpSinkStatsHelper> sinkApp = CreateObject<UdpSinkStatsHelper> ();
      sinkApp->Setup (recvSink);
      nodes.Get (sinkIdx)->AddApplication (sinkApp);
      sinkApp->SetStartTime (Seconds (49.5));
      sinkApp->SetStopTime (Seconds (simTime));
      sinkStatsList.push_back (sinkApp);

      // UDP source
      Ptr<Socket> srcSocket = Socket::CreateSocket (nodes.Get (srcIdx), tid);

      double appStartTime = randStart->GetValue ();
      Ptr<OnOffApplication> onoff = CreateObject<OnOffApplication> ();
      Address remoteAddress (InetSocketAddress (interfaces.GetAddress (sinkIdx), portBase + i));
      onoff->SetAttribute ("Remote", AddressValue (remoteAddress));
      onoff->SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      onoff->SetAttribute ("DataRate", DataRateValue (DataRate ("512kbps")));
      onoff->SetAttribute ("PacketSize", UintegerValue (512));
      onoff->SetAttribute ("StartTime", TimeValue (Seconds (appStartTime)));
      onoff->SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      onoff->SetAttribute ("Socket", PointerValue (srcSocket));
      nodes.Get (srcIdx)->AddApplication (onoff);
      sourceApps.Add (onoff);
    }

  FlowMonitorHelper flowMonHelper;
  Ptr<FlowMonitor> flowMon = nullptr;
  if (enableFlowMonitor)
    {
      flowMon = flowMonHelper.InstallAll ();
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Collect and dump sink statistics
  std::ofstream csvfile;
  csvfile.open (csvFilename, std::ios::out | std::ios::trunc);
  csvfile << "Flow,ReceivedBytes,ReceivedPackets" << std::endl;
  for (uint32_t i = 0; i < sinkStatsList.size (); ++i)
    {
      csvfile << i << "," << sinkStatsList[i]->GetRxBytes () << "," << sinkStatsList[i]->GetRxPackets () << std::endl;
    }
  csvfile.close ();

  // Optionally output Flow Monitor xml
  if (enableFlowMonitor && flowMon)
    {
      flowMon->SerializeToXmlFile ("flowmon.xml", true, true);
    }

  Simulator::Destroy ();
  return 0;
}