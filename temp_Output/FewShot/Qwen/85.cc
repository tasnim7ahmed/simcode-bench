#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"
#include "ns3/applications-module.h"
#include "ns3/antenna-module.h"
#include "ns3/buildings-module.h"
#include "ns3/v2x-channel-condition-model.h"
#include "ns3/three-gpp-vehicular-traces-helper.h"
#include "ns3/three-gpp-channel-model.h"
#include "ns3/three-gpp-spectrum-propagation-loss.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularBeamformingSimulation");

class VehicularScenario : public Object {
public:
    static TypeId GetTypeId(void);
    VehicularScenario();
    void LogMeasurements();

private:
    uint32_t m_numVehicles;
    NodeContainer m_vehicles;
    NetDeviceContainer m_devices;
    MobilityHelper m_mobility;
    ThreeGppChannelModel m_channelModel;
    ThreeGppSpectrumPropagationLossModel m_propagationLoss;
    UniformPlanarArray m_antennaArray;
    std::ofstream m_snrFile;
    std::ofstream m_pathlossFile;
};

TypeId
VehicularScenario::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::VehicularScenario")
        .SetParent<Object>()
        .AddConstructor<VehicularScenario>();
    return tid;
}

VehicularScenario::VehicularScenario() {
    m_numVehicles = 5;
    m_vehicles.Create(m_numVehicles);

    // Setup mobility for urban and highway scenarios
    m_mobility.SetMobilityModel("ns3::SteadyStateRandomWaypointMobilityModel",
                                "MinSpeed", DoubleValue(10.0),
                                "MaxSpeed", DoubleValue(30.0),
                                "MinPause", TimeValue(Seconds(0.5)),
                                "MaxPause", TimeValue(Seconds(2.0)));
    m_mobility.Install(m_vehicles);

    // Install channel condition model
    Ptr<ThreeGppVehicularTracesHelper> traceHelper = CreateObject<ThreeGppVehicularTracesHelper>();
    traceHelper->AssignStreams(m_vehicles, 0);

    // Configure antenna arrays (Uniform Planar Array)
    m_antennaArray.SetNumRows(4);
    m_antennaArray.SetNumColumns(4);
    m_antennaArray.SetAntennaElement(SingleModelSpectrumChannel::CreateAntennaElement());
    m_antennaArray.SetDistanceX(0.5);
    m_antennaArray.SetDistanceY(0.5);

    // Setup channel propagation loss model
    m_propagationLoss.SetChannelModelAttribute("Frequency", DoubleValue(5.9e9));
    m_propagationLoss.SetChannelModelAttribute("Scenario", StringValue("UMa"));
    m_propagationLoss.SetChannelModelAttribute("ChannelConditionModel", PointerValue(traceHelper->GetChannelConditionModel()));
    m_propagationLoss.SetChannelModelAttribute("AntennaPairIsotropicRadiator", BooleanValue(false));

    // Open output files
    m_snrFile.open("snr_output.txt");
    m_pathlossFile.open("pathloss_output.txt");
}

void
VehicularScenario::LogMeasurements() {
    for (uint32_t i = 0; i < m_numVehicles; ++i) {
        for (uint32_t j = i + 1; j < m_numVehicles; ++j) {
            Ptr<MobilityModel> mobA = m_vehicles.Get(i)->GetObject<MobilityModel>();
            Ptr<MobilityModel> mobB = m_vehicles.Get(j)->GetObject<MobilityModel>();

            double snr = 0.0;
            double pathloss = m_propagationLoss.CalcRxPower(0, mobA, mobB);

            // Simple SNR estimation
            double noiseFigure = 5.0; // dB
            double bandwidth = 10e6; // Hz
            double noisePower = -174 + 10 * log10(bandwidth) + noiseFigure; // dBm
            snr = pathloss - noisePower;

            NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() <<
                        " Pair (" << i << "," << j << ") Pathloss: " << pathloss <<
                        " dB SNR: " << snr << " dB");

            m_snrFile << Simulator::Now().GetSeconds() << " " << snr << std::endl;
            m_pathlossFile << Simulator::Now().GetSeconds() << " " << pathloss << std::endl;
        }
    }

    Simulator::Schedule(Seconds(1.0), &VehicularScenario::LogMeasurements, this);
}

int main(int argc, char *argv[]) {
    LogComponentEnable("VehicularScenario", LOG_LEVEL_INFO);

    VehicularScenario scenario;

    scenario.LogMeasurements();

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}