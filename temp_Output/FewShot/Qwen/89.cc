#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

class RandomHarvestingModel : public EnergyHarvester {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("RandomHarvestingModel")
            .SetParent<EnergyHarvester>()
            .SetGroupName("Energy");
        return tid;
    }

    RandomHarvestingModel() : m_minPower(0.0), m_maxPower(0.1) {
        m_uniformRandomVariable = CreateObject<UniformRandomVariable>();
        m_uniformRandomVariable->SetAttribute("Min", DoubleValue(m_minPower));
        m_uniformRandomVariable->SetAttribute("Max", DoubleValue(m_maxPower));
        Simulator::Schedule(Seconds(1.0), &RandomHarvestingModel::UpdateHarvestedPower, this);
    }

    double DoGetPower() const override {
        return m_currentPower;
    }

private:
    void UpdateHarvestedPower() {
        m_currentPower = m_uniformRandomVariable->GetValue();
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s HARVESTED POWER: " << m_currentPower << " W (node " << GetNode()->GetId() << ")");
        if (Simulator::Now() < Seconds(10)) {
            Simulator::Schedule(Seconds(1.0), &RandomHarvestingModel::UpdateHarvestedPower, this);
        }
    }

    double m_minPower;
    double m_maxPower;
    double m_currentPower{0.0};
    Ptr<UniformRandomVariable> m_uniformRandomVariable;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("EnergyEfficientWirelessSimulation", LOG_LEVEL_INFO);

    // Nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Energy source setup
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0)); // 1 Joule initial energy

    // Energy harvester helper
    EnergyHarvesterContainer harvesters;
    for (auto i = 0; i < 2; ++i) {
        Ptr<RandomHarvestingModel> harvester = CreateObject<RandomHarvestingModel>();
        nodes.Get(i)->AddEnergyHarvester(harvester);
        harvesters.Add(harvester);
    }

    // WiFi Radio Energy Model
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));

    EnergyModelContainer deviceEnergyModels = radioEnergyHelper.Install(devices, basicSourceHelper.Install(nodes));

    // Applications
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Tracing residual energy
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("residual-energy-trace.txt");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(node->GetObject<EnergySource>());
        std::ostringstream oss;
        oss << "ResidualEnergy::Node" << node->GetId();
        Simulator::ScheduleNow(&BasicEnergySource::TraceResidualEnergy, source, oss.str(), stream);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print final statistics
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<EnergySource> source = node->GetObject<EnergySource>();
        NS_LOG_UNCOND("Node " << node->GetId() << " Final Residual Energy: " << source->GetRemainingEnergy() << " J");
        NS_LOG_UNCOND("Node " << node->GetId() << " Total Energy Consumed: " << source->GetTotalEnergyConsumption() << " J");
        NS_LOG_UNCOND("Node " << node->GetId() << " Total Harvested Energy: " << source->GetTotalEnergyHarvested() << " J");
    }

    Simulator::Destroy();
    return 0;
}