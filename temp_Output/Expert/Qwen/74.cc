#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

class ManetRoutingExperiment
{
public:
  ManetRoutingExperiment();
  void Configure(int argc, char *argv[]);
  void Run();

private:
  void SetupNodes();
  void SetupMobility();
  void SetupNetwork();
  void SetupRouting();
  void SetupApplications();
  void SetupFlowMonitor();
  void Teardown();

  uint32_t m_nNodes;
  double m_txPower;
  double m_areaWidth;
  double m_areaHeight;
  double m_nodeSpeed;
  std::string m_protocolName;
  bool m_enableMobilityTrace;
  bool m_enableFlowMonitor;
  std::string m_outputCsvFile;

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
};

ManetRoutingExperiment::ManetRoutingExperiment()
    : m_nNodes(50),
      m_txPower(7.5),
      m_areaWidth(300),
      m_areaHeight(1500),
      m_nodeSpeed(20),
      m_protocolName("AODV"),
      m_enableMobilityTrace(false),
      m_enableFlowMonitor(false),
      m_outputCsvFile("manet-performance.csv")
{
}

void
ManetRoutingExperiment::Configure(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("nNodes", "Number of nodes", m_nNodes);
  cmd.AddValue("txPower", "Transmission power in dBm", m_txPower);
  cmd.AddValue("areaWidth", "Simulation area width (m)", m_areaWidth);
  cmd.AddValue("areaHeight", "Simulation area height (m)", m_areaHeight);
  cmd.AddValue("nodeSpeed", "Node speed (m/s)", m_nodeSpeed);
  cmd.AddValue("protocol", "Routing protocol (DSDV, AODV, OLSR, DSR)", m_protocolName);
  cmd.AddValue("enableMobilityTrace", "Enable mobility tracing", m_enableMobilityTrace);
  cmd.AddValue("enableFlowMonitor", "Enable flow monitor", m_enableFlowMonitor);
  cmd.AddValue("outputCsvFile", "Output CSV file name", m_outputCsvFile);
  cmd.Parse(argc, argv);
}

void
ManetRoutingExperiment::SetupNodes()
{
  m_nodes.Create(m_nNodes);
}

void
ManetRoutingExperiment::SetupMobility()
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaWidth) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaHeight) + "]"));

  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(m_nodeSpeed) + "]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install(m_nodes);

  if (m_enableMobilityTrace)
    {
      AsciiTraceHelper ascii;
      Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("manet-mobility.tr");
      mobility.EnableAsciiAll(stream);
    }
}

void
ManetRoutingExperiment::SetupNetwork()
{
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");

  m_devices = wifi.Install(wifiPhy, wifiMac, m_nodes);

  // Set TX Power
  for (auto nodeIt = m_nodes.Begin(); nodeIt != m_nodes.End(); ++nodeIt)
    {
      Ptr<NetDevice> dev = *(m_devices.Begin());
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
      if (wifiDev)
        {
          wifiDev->GetPhy()->SetTxPowerStart(m_txPower);
          wifiDev->GetPhy()->SetTxPowerEnd(m_txPower);
        }
      std::advance(devicesIt, 1); // devicesIt should be declared or replaced
    }

  InternetStackHelper stack;

  if (m_protocolName == "DSDV")
    {
      DsdvHelper dsdv;
      stack.SetRoutingHelper(dsdv);
    }
  else if (m_protocolName == "AODV")
    {
      AodvHelper aodv;
      stack.SetRoutingHelper(aodv);
    }
  else if (m_protocolName == "OLSR")
    {
      OlsrHelper olsr;
      stack.SetRoutingHelper(olsr);
    }
  else if (m_protocolName == "DSR")
    {
      DsrHelper dsr;
      stack.SetRoutingHelper(dsr);
    }
  else
    {
      NS_ABORT_MSG("Unknown routing protocol: " << m_protocolName);
    }

  stack.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.0.0");
  m_interfaces = address.Assign(m_devices);
}

void
ManetRoutingExperiment::SetupApplications()
{
  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(m_nodes.Get(m_nNodes - 1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(200.0));

  UdpEchoClientHelper echoClient(m_interfaces.GetAddress(m_nNodes - 1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  for (uint32_t i = 0; i < 10; ++i)
    {
      Time startTime = Seconds(50.0 + (i / 10.0)); // Random between 50-51
      ApplicationContainer clientApps = echoClient.Install(m_nodes.Get(i));
      clientApps.Start(startTime);
      clientApps.Stop(Seconds(200.0));
    }
}

void
ManetRoutingExperiment::SetupFlowMonitor()
{
  if (!m_enableFlowMonitor)
    return;

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(200.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::ofstream ofs(m_outputCsvFile);
  ofs.precision(8);
  monitor->SerializeToXmlStream(ofs, 0, false, false);
  ofs.close();

  Simulator::Destroy();
}

void
ManetRoutingExperiment::Teardown()
{
  m_nodes.Clear();
  m_devices.Clear();
  m_interfaces.Clear();
}

void
ManetRoutingExperiment::Run()
{
  SetupNodes();
  SetupMobility();
  SetupNetwork();
  SetupApplications();
  SetupFlowMonitor();
  Teardown();
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("ManetRoutingComparison", LOG_LEVEL_INFO);
  ManetRoutingExperiment experiment;
  experiment.Configure(argc, argv);
  experiment.Run();
  return 0;
}