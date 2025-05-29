#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/olsr-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSN");

class SensorApplication : public Application
{
public:
  SensorApplication();
  virtual ~SensorApplication();

  void Setup(uint32_t packetSize, DataRate dataRate, Address dstAddress, uint16_t dstPort);
  void SetEnergySource(Ptr<EnergySource> source);

private:
  virtual void StartApplication();
  virtual void StopApplication();

  void SendPacket();
  void ReceiveAcknowledgement(Ptr<Socket> socket);
  void CheckEnergyAndSend();

  Ptr<Socket> m_socket = nullptr;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running = false;
  Ptr<EnergySource> m_energySource = nullptr;
  double m_energyConsumptionPerPacket = 0.001;
};

SensorApplication::SensorApplication()
    : m_socket(nullptr),
      m_peerPort(0),
      m_packetSize(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_energySource(nullptr)
{
}

SensorApplication::~SensorApplication()
{
  m_socket = nullptr;
}

void SensorApplication::Setup(uint32_t packetSize, DataRate dataRate, Address dstAddress, uint16_t dstPort)
{
  m_packetSize = packetSize;
  m_dataRate = dataRate;
  m_peerAddress = dstAddress;
  m_peerPort = dstPort;
}

void SensorApplication::SetEnergySource(Ptr<EnergySource> source)
{
    m_energySource = source;
}

void SensorApplication::StartApplication()
{
  m_running = true;
  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  m_socket->Bind();
  m_socket->Connect(m_peerAddress);
  m_socket->SetRecvCallback(MakeCallback(&SensorApplication::ReceiveAcknowledgement, this));

  Simulator::Schedule(Seconds(1.0), &SensorApplication::CheckEnergyAndSend, this);
}

void SensorApplication::StopApplication()
{
  m_running = false;
  Simulator::Cancel(m_sendEvent);
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void SensorApplication::CheckEnergyAndSend()
{
    if (!m_running) return;
    if (m_energySource->GetRemainingEnergy() > m_energyConsumptionPerPacket) {
        SendPacket();
    } else {
        NS_LOG_INFO("Node " << GetNode()->GetId() << " out of energy. Stopping.");
        StopApplication();
        return;
    }
    Simulator::Schedule(Seconds(1.0), &SensorApplication::CheckEnergyAndSend, this);

}

void SensorApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  m_energySource->DecreaseEnergy(m_energyConsumptionPerPacket);
  NS_LOG_INFO("Node " << GetNode()->GetId() << " sends packet to sink. Remaining energy: " << m_energySource->GetRemainingEnergy());
}

void SensorApplication::ReceiveAcknowledgement(Ptr<Socket> socket)
{
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    uint32_t numNodes = 25;
    double simulationTime = 100;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue("numNodes", "Number of sensor nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("WSN", LOG_LEVEL_INFO);
    }

    NodeContainer sinkNode;
    sinkNode.Create(1);

    NodeContainer sensorNodes;
    sensorNodes.Create(numNodes);

    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("wsen-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensorNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sinkNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(5.0),
                                    "GridWidth", UintegerValue(5),
                                    "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sinkNode);

    InternetStackHelper internet;
    internet.Install(sinkNode);
    internet.Install(sensorNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(sensorDevices);
    Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Energy Model
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("NominalVoltage", DoubleValue(3.7));

    // Install energy source
    EnergySourceContainer sources = basicSourceHelper.Install(sensorNodes);

    // Create a device energy model
    WifiRadioEnergyModelHelper radioEnergyModelHelper;
    radioEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.0197));
    radioEnergyModelHelper.Set("IdleCurrentA", DoubleValue(0.00188));
    radioEnergyModelHelper.Set("SleepCurrentA", DoubleValue(0.000009));
    radioEnergyModelHelper.Install(sensorDevices, sources);

    Ptr<EnergySource> sinkSource = basicSourceHelper.Install(sinkNode)->Get(0);
    WifiRadioEnergyModelHelper sinkRadioEnergyModelHelper;
    sinkRadioEnergyModelHelper.Install(sinkDevice, EnergySourceContainer(sinkSource));


    // Sink Application (Dummy)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer sinkApps = echoServer.Install(sinkNode.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simulationTime));


    // Sensor Application
    uint16_t port = 9;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = sensorNodes.Get(i);
        Ptr<SensorApplication> app = CreateObject<SensorApplication>();
        app->Setup(1024, DataRate("250Kbps"), sinkInterface.GetAddress(0), port);
        app->SetEnergySource(sources.Get(i));
        node->AddApplication(app);
        app->SetStartTime(Seconds(2.0 + i * 0.1));
        app->SetStopTime(Seconds(simulationTime));
    }


    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}