#include "VanGoghDeployer.h"

const int VALID_COMMAND = 0;

std::string getInput()
{
	std::string input;
	while(true)
	{
		std::cin >> input;

		if(input.length() == 0 && input != "\n")
		{
			std::cout << "Antwoord moet uit minimaal 1 karakter bestaan." << std::endl;;
		}
		else
		{
			break;
		}
	}
	return input;
}

std::string executeCommand(const ssh_session& t_session, const char* t_command) {
	ssh_channel channel = ssh_channel_new(t_session);
	if (channel == nullptr)
	{
		std::cout << "Er kon geen channel gemaakt worden op de remote host. Check uw server instellingen." << std::endl;
		ssh_disconnect(t_session);
		ssh_free(t_session);
		getchar();
		exit(-1);
	}

	auto response = ssh_channel_open_session(channel);
	if (response != SSH_OK)
	{
		std::cout << "Er kon geen channel geopend worden op de remote host. Check uw server instellingen." << std::endl;
		ssh_channel_free(channel);
		ssh_disconnect(t_session);
		ssh_free(t_session);
		getchar();
		exit(-1);
	}

	response = ssh_channel_request_exec(channel, t_command);
	if (response != SSH_OK)
	{
		std::cout << "Commando '" << t_command << "' kon niet worden uitgevoerd." << std::endl;
		ssh_channel_free(channel);
		ssh_disconnect(t_session);
		ssh_free(t_session);
		getchar();
		exit(-1);
	}

	char buffer[256];
	std::stringstream stringStream;
	auto nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	while (nbytes > 0)
	{
		stringStream.write(buffer, nbytes);
		nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	}

	ssh_channel_send_eof(channel);
	ssh_channel_close(channel);
	ssh_channel_free(channel);

	return stringStream.str();
}

bool commandFailed(std::string t_response)
{
	t_response.erase(std::remove(t_response.begin(), t_response.end(), '\n'), t_response.end());
	return atoi(t_response.c_str()) != VALID_COMMAND;
}

int main()
{
	std::cout << "Welkom bij de deployment tool voor de website van het Van Gogh National Park." << std::endl;
	std::cout << "Voer het commando EXIT in om elk moment te stoppen." << std::endl;

	ssh_session sshSession = ssh_new();
	if (sshSession == nullptr)
	{
		std::cout << "SSL Session kon niet aangemaakt worden." << std::endl;
		exit(-1);
	}

	std::cout << "Geef het host adress op: (IP of Webadress, zonder poort)" << std::endl;
	auto host = getInput();

	ssh_options_set(sshSession, SSH_OPTIONS_HOST, host.c_str());

	std::cout << "Geef de SSH poort op van host '" << host << "':" << std::endl;
	auto port = atoi((getInput()).c_str());
	auto verbosity = SSH_LOG_NONE;

	ssh_options_set(sshSession, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);

	std::cout << "Connectie maken..." << std::endl;
	auto response = ssh_connect(sshSession);
	if(response != SSH_OK)
	{
		std::cout << "Er kon geen connectie gemaakt worden met host '" << host << "' op poort '" << port << "'. Check de gegevens van uw server en herstart het programma." << std::endl;
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}

	std::cout << "Connectie succesvol." << std::endl;
	response = -1;
	while(response != SSH_AUTH_SUCCESS)
	{
		std::cout << "Geef uw gebruikersnaam op: " << std::endl;
		auto username = getInput();

		std::cout << "Geef uw wachtwoord op: " << std::endl;
		auto password = getInput();

		response = ssh_userauth_password(sshSession, username.c_str(), password.c_str());

		if(response == SSH_AUTH_DENIED || response == SSH_AUTH_ERROR)
		{
			std::cout << "Gebruikersnaam en/of wachtwoord incorrect, probeer opnieuw." << std::endl;
		}
	}

	std::cout << "Authenticatie succesvol" << std::endl;
	std::cout << "Deployment begint." << std::endl;

	std::cout << "GIT check..." << std::endl;
	auto status = executeCommand(sshSession, "command -v git; echo $?");

	if(commandFailed(status))
	{
		std::cout << "GIT is niet geinstalleerd, is niet gestart of de gebruiker heeft geen toegang." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}

	std::cout << "MySQL check..." << std::endl;
	status = executeCommand(sshSession, "command -v mysql; echo $?");

	if (commandFailed(status))
	{
		std::cout << "MySQL is niet geinstalleerd, is niet gestart of de gebruiker heeft geen toegang." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}

	ssh_disconnect(sshSession);
	ssh_free(sshSession);
	return 0;
}
