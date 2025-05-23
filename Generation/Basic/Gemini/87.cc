#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/data-collector-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPerformanceExperiment");

class ExperimentCallbacks : public Object
{
public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("ExperimentCallbacks")
                                .SetParent<Object>()
                                .AddConstructor<ExperimentCallbacks>();
        return tid;
    }

    Ptr<CounterCalculator> m_packetsTx;
    Ptr<CounterCalculator> m_packetsRx;
    Ptr<MinMaxAvgCalculator> m_delayCalc;

    void OnOffTxTrace(Ptr<const Packet> packet)
    {
        m_packetsTx->Event();
    }

    void PacketSinkRxTrace(Ptr<const Packet> packet, const Address &srcAddress)
    {
        m_packetsRx->Event();

        Time txTime = packet->GetTxTime();
        if (txTime.IsGreaterThan(Seconds(0)))
        {
            Time delay = Simulator::Now() - txTime;
            m_delayCalc->Add(delay.GetSeconds());
        }
    }
};

int main(int argc, char *argv[])
{
    double distance = 10.0;
    std::string format = "OMNeT";
    uint32_t runId = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance between the two Wi-Fi nodes in meters", distance);
    cmd.AddValue("format", "Output format for data collector (OMNeT or SQLite)", format);
    cmd.AddValue("runId", "Run identifier for data collection", runId);
    cmd.Parse(argc, argv);

    if (format != "OMNeT" && format != "SQLite")
    {
        NS_FATAL_ERROR("Invalid output format: " << format << ". Use 'OMNeT' or 'SQLite'.");
    }

    LogComponentEnable("WifiPerformanceExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);
    LogComponentEnable("DataCollector", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("ns3::DsssRate1Mbps"),
                                 "ControlMode", StringValue("ns3::DsssRate1Mbps"));

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
    ApplicationContainer serverApps = sink.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(11.0));

    Ptr<DataCollector> dataCollector = CreateObject<DataCollector>();
    dataCollector->AddScalar("RunId", runId);
    dataCollector->AddScalar("Distance", distance);

    if (format == "OMNeT")
    {
        dataCollector->RegisterDataOutputHelper(OmnetDataOutputHelper());
    }
    else if (format == "SQLite")
    {
        dataCollector->RegisterDataOutputHelper(SqliteDataOutputHelper());
    }

    Ptr<ExperimentCallbacks> callbacks = CreateObject<ExperimentCallbacks>();
    callbacks->m_packetsTx = CreateObject<CounterCalculator>();
    callbacks->m_packetsTx->SetKey("PacketsTransmitted");
    callbacks->m_packetsTx->SetUnit("packets");
    dataCollector->AddCalculator(callbacks->m_packetsTx);

    callbacks->m_packetsRx = CreateObject<CounterCalculator>();
    callbacks->m_packetsRx->SetKey("PacketsReceived");
    callbacks->m_packetsRx->SetUnit("packets");
    dataCollector->AddCalculator(callbacks->m_packetsRx);

    callbacks->m_delayCalc = CreateObject<MinMaxAvgCalculator>();
    callbacks->m_delayCalc->SetKey("EndToEndDelay");
    callbacks->m_delayCalc->SetUnit("seconds");
    dataCollector->AddCalculator(callbacks->m_delayCalc);

    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::OnOffApplication/Tx", MakeCallback(&ExperimentCallbacks::OnOffTxTrace, callbacks));
    Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&ExperimentCallbacks::PacketSinkRxTrace, callbacks));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}