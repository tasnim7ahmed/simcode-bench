#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

void LogEnergyStats(Ptr<BasicEnergySource> energySource,
                    Ptr<BasicEnergyHarvester> harvester,
                    double consumed,
                    std::ofstream &statfile, 
                    double time)
{
    double residual = energySource->GetRemainingEnergy();
    double harvested = harvester->GetHarvestedPower();
    statfile << std::fixed << std::setprecision(6) 
             << time << "\t" 
             << residual << "\t" 
             << harvested << "\t" 
             << consumed << std::endl;
}

double lastResidual = 1.0;
double totalConsumed = 0.0;
double totalHarvested = 0.0;

void PeriodicEnergyStats(Ptr<BasicEnergySource> energySource,
                         Ptr<BasicEnergyHarvester> harvester,
                         std::ofstream *statfile,
                         double interval, 
                         double stop)
{
    double residual = energySource->GetRemainingEnergy();
    double consumedStep = lastResidual - residual;
    double harvestedStep = harvester->GetHarvestedPower() * interval;
    lastResidual = residual;
    totalConsumed += consumedStep;
    totalHarvested += harvestedStep;
    LogEnergyStats(energySource, harvester, totalConsumed, *statfile, Simulator::Now().GetSeconds());
    if (Simulator::Now().GetSeconds() + interval < stop + 1e-6)
    {
        Simulator::Schedule(Seconds(interval), &PeriodicEnergyStats, 
                energySource, harvester, statfile, interval, stop);
    }
}

class RandomHarvester : public BasicEnergyHarvester
{
public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("RandomHarvester")
            .SetParent<BasicEnergyHarvester>()
            .AddConstructor<RandomHarvester>();
        return tid;
    }
    RandomHarvester() : m_rng(CreateObject<UniformRandomVariable>()) {}
    virtual double DoGetHarvestedPower(void) const override
    {
        return m_lastPower;
    }
    virtual void DoInitialize(void) override
    {
        m_lastPower = m_rng->GetValue(0.0, 0.1);
        Simulator::Schedule(Seconds(1.0), &RandomHarvester::UpdatePower, this);
        BasicEnergyHarvester::DoInitialize();
    }
private:
    void UpdatePower()
    {
        m_lastPower = m_rng->GetValue(0.0, 0.1);
        Simulator::Schedule(Seconds(1.0), &RandomHarvester::UpdatePower, this);
    }
    mutable Ptr<UniformRandomVariable> m_rng;
    mutable double m_lastPower = 0.0;
};
NS_OBJECT_ENSURE_REGISTERED(RandomHarvester);

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    double simTime = 10.0;
    double packetInterval = 1.0;
    double txTime = 0.0023;

    NodeContainer nodes;
    nodes.Create(2);

    // Mobility (static)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(0.0, 0.0, 0.0));
    pos->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // WiFi setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("energy-wifi");
    mac.SetType("ns3::AdhocWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Energy Source and Harvester Setup
    EnergySourceContainer sources;
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0));
    energySourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));
    sources = energySourceHelper.Install(nodes);

    // Setup random harvester on each energy source
    ObjectFactory harvesterFactory;
    harvesterFactory.SetTypeId("RandomHarvester");
    harvesterFactory.Set("Period", TimeValue(Seconds(1.0)));
    harvesterFactory.Set("HarvestablePower", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=0.1]"));

    BasicEnergyHarvesterContainer harvesters;
    Ptr<RandomHarvester> harv1 = CreateObject<RandomHarvester>();
    Ptr<RandomHarvester> harv2 = CreateObject<RandomHarvester>();
    sources.Get(0)->AddEnergyHarvester(harv1);
    sources.Get(1)->AddEnergyHarvester(harv2);
    harvesters.Add(harv1);
    harvesters.Add(harv2);

    // Energy model for WiFi
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));
    radioEnergyHelper.Install(devices, sources);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    // Application: UDP sender
    uint16_t port = 4000;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(ifaces.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate((uint32_t)(8.0 / txTime))));
    onoff.SetAttribute("PacketSize", UintegerValue(8)); // 8 bytes (arbitrary, duration is forced by data rate)
    ApplicationContainer senderApp = onoff.Install(nodes.Get(0));
    senderApp.Start(Seconds(0.0));
    senderApp.Stop(Seconds(simTime));

    // Receiver
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime + 1));

    // Setup logging to file
    std::ofstream statfile("receiver-energy-stats.txt");
    statfile << "Time\tResidual_J\tHarvested_W\tTotalConsumed_J" << std::endl;
    Ptr<BasicEnergySource> receiverEnergy = sources.Get(1)->GetObject<BasicEnergySource>();
    Ptr<RandomHarvester> receiverHarvester = DynamicCast<RandomHarvester>(harvesters.Get(1));

    Simulator::Schedule(Seconds(0.0), &PeriodicEnergyStats, receiverEnergy,
                        receiverHarvester, &statfile, 1.0, simTime);

    Simulator::Stop(Seconds(simTime + 0.5));
    Simulator::Run();

    double finalResidual = receiverEnergy->GetRemainingEnergy();
    double totalEnergyHarvested = totalHarvested;
    double totalEnergyConsumed = 1.0 - finalResidual + totalHarvested - (receiverEnergy->GetRemainingEnergy() - finalResidual);

    std::cout << "----- RECEIVER NODE ENERGY STATISTICS -----" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Initial energy (J):      1.000000" << std::endl;
    std::cout << "Residual energy (J):     " << finalResidual << std::endl;
    std::cout << "Total energy consumed (J): " << totalConsumed << std::endl;
    std::cout << "Total energy harvested (J): " << totalHarvested << std::endl;

    Simulator::Destroy();
    statfile.close();
    return 0;
}