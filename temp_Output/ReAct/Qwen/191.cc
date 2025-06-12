#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkSimulation");

class SensorNodeApplication : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    return TypeId ("SensorNodeApplication")
      .SetParent<Application> ()
      .SetGroupName ("Tutorial")
      .AddConstructor<SensorNodeApplication> ();
  }

  SensorNodeApplication () {}
  virtual ~SensorNodeApplication () {}

protected:
  void DoInitialize (void) override
  {
    Application::DoInitialize ();
    m_sendEvent = Simulator::Schedule (Seconds(0.0), &SensorNodeApplication::SendPacket, this);
  }

  void DoDispose (void) override
  {
    Application::DoDispose ();
    m_socket = nullptr;
    Simulator::Cancel (m_sendEvent);
  }

private:
  void StartApplication (void) override
  {
    if (m_socket == nullptr)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
      m_socket->Bind (local);
    }
  }

  void StopApplication (void) override
  {
    Simulator::Cancel (m_sendEvent);
  }

  void SendPacket (void)
  {
    Ptr<Packet> packet = Create<Packet> (100); // 100-byte packet
    m_socket->SendTo (packet, 0, InetSocketAddress (m_sinkAddress, 9));

    Time nextInterval = Seconds(5.0);
    m_sendEvent = Simulator::Schedule (nextInterval, &SensorNodeApplication::SendPacket, this);
  }

  Ipv4Address m_sinkAddress;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
  uint32_t gridDim = 5;
  double areaSize = 100.0;
  double txPower = 10.0;
  double initialEnergyJ = 100.0;
  double dataRate = 1e6;
  uint32_t numNodes = gridDim * gridDim;
  bool verbose = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("gridDim", "Grid dimension (number of nodes per side)", gridDim);
  cmd.AddValue ("areaSize", "Side length of square grid in meters", areaSize);
  cmd.AddValue ("txPower", "Transmit power level (dBm)", txPower);
  cmd.AddValue ("initialEnergyJ", "Initial energy per node (Joules)", initialEnergyJ);
  cmd.AddValue ("dataRate", "Data rate for sensor packets (bps)", dataRate);
  cmd.AddValue ("verbose", "Enable/disable verbose output", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    LogComponentEnable ("SensorNodeApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(numNodes);

  NodeContainer sinkNode;
  sinkNode.Create(1);

  YansWifiPhyHelper phy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("DsssRate1Mbps"),
                               "ControlMode", StringValue("DsssRate1Mbps"));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
  NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sinkNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(areaSize / gridDim),
                                "DeltaY", DoubleValue(areaSize / gridDim),
                                "GridWidth", UintegerValue(gridDim),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(areaSize / 2),
                                "MinY", DoubleValue(areaSize / 2),
                                "DeltaX", DoubleValue(0),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(sinkNode);

  InternetStackHelper stack;
  stack.Install(nodes);
  stack.Install(sinkNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  EnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergyJ));
  energySourceHelper.Set("BasicEnergySourceSupplyVoltageV", DoubleValue(3.0));
  energySourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.350)); // Typical transmit current
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.190)); // Typical receive current
  radioEnergyHelper.Install(devices, nodes.Get(0)->GetObject<EnergySourceContainer>()->Get(0));

  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = packetSinkHelper.Install(sinkNode);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(3600.0));

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<SensorNodeApplication> app = CreateObject<SensorNodeApplication>();
    app->SetSinkAddress(sinkInterface.GetAddress(0));
    node->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(3600.0));
  }

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/State/RxOk", MakeCallback([](std::string path, Ptr<const Packet> p){
    // Handle received packet if needed
  }));

  Simulator::Stop(Seconds(3600.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}