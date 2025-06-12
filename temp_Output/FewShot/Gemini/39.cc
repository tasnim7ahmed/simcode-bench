#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifi");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string distanceStep = "10";
    std::string timeStep = "1";
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("distanceStep", "Distance step in meters", distanceStep);
    cmd.AddValue("timeStep", "Time step in seconds", timeStep);
    cmd.AddValue("enableLogging", "Enable rate change logging", enableLogging);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Configure Minstrel rate adaptation
    if (enableLogging) {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/DcaTxop/CwMin", StringValue("15"));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/DcaTxop/CwMax", StringValue("1023"));
    }
    
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("Mmrr"));
    
    if (enableLogging){
          Config::SetDefault ("ns3::MinstrelWifiManager::EnableAscendancy", BooleanValue (true));
          Config::SetDefault ("ns3::MinstrelWifiManager::EnableStats", BooleanValue (true));
          Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Minstrel/ChangeMode", MakeCallback (&
          {
            void cb(std::string path, WifiMode newMode, WifiMode oldMode){
                std::cout << Simulator::Now().As(Seconds) << " Rate Change Path:" << path << " Old Mode: " << oldMode << " New Mode " << newMode << std::endl;
            }
            return cb;
          }()));
    }


    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(10.0),
                                    "GridWidth", UintegerValue(3),
                                    "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));


    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNode);

    Ptr<ConstantPositionMobilityModel> staMobility = staNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    staMobility->SetPosition(Vector(10.0, 0.0, 0.0));
    
    // Install internet stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevice);
    interfaces = address.Assign(staDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR traffic from AP to STA
    uint16_t port = 9;
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("Interval", TimeValue(Time("0.0000025s"))); //400Mbps
    client.SetAttribute("MaxPackets", UintegerValue(1000000));

    ApplicationContainer clientApps = client.Install(apNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(staNode.Get(0));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(10.0));
    
    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Simulation time
    double simTime = 10.0;
    
    //Configure periodic position changes
    double distStep = std::stod(distanceStep);
    double tmStep = std::stod(timeStep);
    
    Simulator::Schedule(Seconds(0.0), [&, distStep, tmStep, staMobility](){
        double currentDistance = staMobility->GetPosition().x;
        
        for(double i = 0; i < 80; ++i){
            currentDistance += distStep;
            Simulator::Schedule(Seconds(tmStep * (i+1)), [currentDistance, staMobility](){
                staMobility->SetPosition(Vector(currentDistance, 0.0, 0.0));
                NS_LOG_UNCOND("STA moved to position: " << staMobility->GetPosition());
            });
        }
    });

    Simulator::Stop(Seconds(simTime));

    Simulator::Run();

    // Calculate throughput
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream out("wifi-throughput.dat");
    out << "#Time\tThroughput\tDistance" << std::endl;
    
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1)) {
            double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            double distance = staMobility->GetPosition().x;
            out << Simulator::Now().GetSeconds() << "\t" << throughput << "\t" << distance << std::endl;
            NS_LOG_UNCOND("Flow ID: " << iter->first << ", Throughput: " << throughput << " Mbps, Distance: " << distance << " meters");
        }
    }

    out.close();

    // Generate Gnuplot script
    Gnuplot gnuplot("wifi-throughput.png");
    gnuplot.SetTitle("Wi-Fi Throughput vs. Distance");
    gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");
    gnuplot.AddDataset("wifi-throughput.dat", "Using 1:2");

    std::ofstream gnuplotScript("wifi-throughput.gnuplot");
    gnuplotScript << gnuplot.GetPlotScript();
    gnuplotScript.close();

    Simulator::Destroy();
    return 0;
}