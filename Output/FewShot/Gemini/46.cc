#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/on-off-application.h"
#include "ns3/command-line.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    double simulationTime = 10.0;
    std::string dataRate = "54Mbps";
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 0; // Unlimited packets
    double txPowerStart = 16.0206; // dBm
    double txPowerEnd = 16.0206;   // dBm
    double txPowerLevels = 1;
    std::string energyTraceFile = "energy-trace.txt";
    std::string stateTraceFile = "state-trace.txt";
    double initialEnergy = 100.0;
    double txCurrentA = 0.017;
    double rxCurrentA = 0.015;
    double idleCurrentA = 0.0015;
    double sleepCurrentA = 0.00005;
    double transitionTimeS = 0.00001;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("packetSize", "Size of packets", packetSize);
    cmd.AddValue("maxPackets", "Max Packets to send", maxPackets);
    cmd.AddValue("txPowerStart", "Tx Power Start (dBm)", txPowerStart);
    cmd.AddValue("txPowerEnd", "Tx Power End (dBm)", txPowerEnd);
    cmd.AddValue("txPowerLevels", "Number of Tx Power Levels", txPowerLevels);
    cmd.AddValue("energyTraceFile", "Energy trace file", energyTraceFile);
    cmd.AddValue("stateTraceFile", "State trace file", stateTraceFile);
    cmd.AddValue("initialEnergy", "Initial energy in Joules", initialEnergy);
    cmd.AddValue("txCurrentA", "Transmit current in Amperes", txCurrentA);
    cmd.AddValue("rxCurrentA", "Receive current in Amperes", rxCurrentA);
    cmd.AddValue("idleCurrentA", "Idle current in Amperes", idleCurrentA);
    cmd.AddValue("sleepCurrentA", "Sleep current in Amperes", sleepCurrentA);
    cmd.AddValue("transitionTimeS", "Transition time in seconds", transitionTimeS);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("EnergySource", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address());
    onOffHelper.SetConstantRate(DataRate(dataRate));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    onOffHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));

    ApplicationContainer onOffApp = onOffHelper.Install(nodes.Get(0));
    onOffApp.Start(Seconds(1.0));
    onOffApp.Stop(Seconds(simulationTime - 1));

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), 9));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), 9));
    onOffHelper.SetAttribute("Remote", remoteAddress);

    Ptr<BasicEnergySource> energySource = CreateObject<BasicEnergySource>();
    energySource->SetInitialEnergy(initialEnergy);

    EnergySourceContainer energySourceContainer;
    energySourceContainer.Add(energySource);

    WifiRadioEnergyModelHelper radioEnergyModelHelper;
    radioEnergyModelHelper.Set("TxCurrentA", DoubleValue(txCurrentA));
    radioEnergyModelHelper.Set("RxCurrentA", DoubleValue(rxCurrentA));
    radioEnergyModelHelper.Set("IdleCurrentA", DoubleValue(idleCurrentA));
    radioEnergyModelHelper.Set("SleepCurrentA", DoubleValue(sleepCurrentA));
    radioEnergyModelHelper.Set("TransitionTimeS", DoubleValue(transitionTimeS));

    DeviceEnergyModelContainer deviceEnergyModels = radioEnergyModelHelper.Install(devices.Get(0), energySourceContainer);

    energySource->TraceConnectWithoutContext("RemainingEnergy", energyTraceFile, MakeBoundCallback(&ns3::TracedValueCallback::Bind<double>(&ns3::TracedValueCallback::PrintDouble)));

    deviceEnergyModels.Get(0)->TraceConnectWithoutContext("State", stateTraceFile, MakeBoundCallback(&ns3::TracedValueCallback::Bind<std::string>(&ns3::TracedValueCallback::PrintString)));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}